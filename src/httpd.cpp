#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <time.h>
#include <errno.h>
#include <limits.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#include "httpd.hpp"
#include "exception.hpp"

#include "util/json.hpp"

namespace Springy{

Httpd::Httpd(){
    this->running = false;
}
Httpd& Httpd::init(Settings *config, Springy::LibC::ILibC *libc){
    this->config = config;
    this->libc   = libc;
    this->operations = new Springy::FsOps::Local(config, libc);
    return *this;
}
void Httpd::start(){
    if(this->running){ return; }

    this->libc->memset(__LINE__, &this->data, '\0', sizeof(this->data));
    this->data.shall_run = true;

    int server_port = 8787;
    if(this->config->httpdPort != 0){
        server_port = this->config->httpdPort;
    }
    std::cout << "server port: " << server_port << std::endl;

    char serveraddr[64] = {'\0'};
    //snprintf(serveraddr, sizeof(serveraddr)-1, "0.0.0.0:%d", server_port);
    snprintf(serveraddr, sizeof(serveraddr)-1, "%d", server_port);

    mg_mgr_init(&this->data.mgr, NULL);
    mg_bind_opts opts;
    memset(&opts, '\0', sizeof(opts));
    opts.user_data = (void*)this;
    this->data.serverSocket = mg_bind_opt(&this->data.mgr, serveraddr, Httpd::ev_handler, opts);
    if(this->data.serverSocket==NULL){
        mg_mgr_free(&this->data.mgr);
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "binding to port failed: " << serveraddr;
    }
    mg_set_protocol_http_websocket(this->data.serverSocket);

    this->data.s_http_server_opts.document_root = NULL;

#ifdef NS_ENABLE_SSL
    if(mhdd->server_cert_pem!=NULL){
      const char *err_str = mg_set_ssl(nc, ssl_cert, NULL);
      if (err_str != NULL) {
        mg_mgr_free(&this->data.mgr);
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "Error loading SSL cert: " << err_str;
      }
    }
#endif
    /*mgr.hexdump_file = argv[++i];
    s_http_server_opts.document_root = argv[++i];
    s_http_server_opts.auth_domain = argv[++i];
    s_http_server_opts.global_auth_file = argv[++i];
    s_http_server_opts.per_directory_auth_file = argv[++i];
    s_http_server_opts.url_rewrites = argv[++i];
    */

    if(pthread_create(&this->data.thHttpServer, NULL, Httpd::server, this)!=0) {
        mg_mgr_free(&this->data.mgr);
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "creating server thread failed";
    }

    this->running = true;
}
void Httpd::stop(){
    this->running = false;
    {
        Synchronized sync(this->data.shall_run, Synchronized::LockType::WRITE);
        this->data.shall_run = false;
    }
    pthread_join(this->data.thHttpServer, NULL);
}
Httpd::~Httpd(){
    if(this->running){
        this->stop();
    }
}



void Httpd::sendResponse(std::string response, struct mg_connection *nc, struct http_message *hm){
    std::string contentType("application/json");
    char callback[256]   = {'\0'};

    mg_get_http_var(&hm->query_string, "callback", callback, sizeof(callback));

    if(strlen(callback)>0){
        response = std::string(callback)+"("+response+");";
        contentType = "application/javascript";
    }

    struct mg_str *acrh = mg_get_http_header(hm, "Access-Control-Request-Headers");
    std::string accessControlRequestHeader;
    if(acrh){ accessControlRequestHeader=std::string(acrh->p, acrh->len); }

    /* Send headers */
    mg_printf(nc, "HTTP/1.1 200 OK\r\n");
    mg_printf(nc, "Content-Length: %lu\r\n", response.length());
    mg_printf(nc, "Content-Type: %s\r\n", contentType.c_str());
    mg_printf(nc, "Access-Control-Allow-Origin: *\r\n");
    mg_printf(nc, "Access-Control-Allow-Methods: POST, GET, OPTIONS, HEAD, PUT, DELETE\r\n");
    if(accessControlRequestHeader.length()>0){
        mg_printf(nc, "Access-Control-Allow-Headers: %s\r\n", accessControlRequestHeader.c_str());
    }
    //mg_printf(nc, "Access-Control-Max-Age: 1728000\r\n");
    mg_printf(nc, "Cache-Control: no-cache, no-store, must-revalidate\r\n");
    mg_printf(nc, "Pragma: no-cache\r\n");
    mg_printf(nc, "Expires: 0\r\n");
    mg_printf(nc, "Transfer-Encoding: chunked\r\n\r\n");

    mg_printf_http_chunk(nc, response.c_str());

    mg_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */
}

void Httpd::handle_directory(int what, struct mg_connection *nc, struct http_message *hm){
  char directory[8192] = {'\0'};
  char vmountpoint[8192] = {'\0'};

  std::stringstream ss;

  /* Get form variables */
  do{
      mg_get_http_var(&hm->query_string, "vmountpoint", vmountpoint, sizeof(vmountpoint));
      if(strlen(vmountpoint)<=0){
          vmountpoint[0] = '/';
      }
      else if(vmountpoint[0] != '/'){
          ss << "{success: false, message: \"given vmountpoint parameter must start with a slash(/)-character\", data: null}";
          break;
      }

      if(mg_get_http_var(&hm->query_string, "directory", directory, sizeof(directory))<=0){
          ss << "{success: false, message: \"invalid (length) directory parameter given\", data: null}";
          break;
      }
      if(directory[0]!='/'){
          ss << "{success: false, message: \"invalid directory - only absolute paths starting with / are allowed\", data: null}";
          break;
      }

      errno = 0;

      switch(what){
          case 1:
          {
              try{
                  Springy::Util::Uri dir(directory);
                  if(dir.protocol()==""){ dir = Springy::Util::Uri(std::string("file://")+directory); }
                  this->config->volumes.addVolume(dir, vmountpoint);
              }catch(...){
                  errno = EINVAL;
              }
          }
          break;
          case 0:
          {
              try{
                  Springy::Util::Uri dir(directory);
                  if(dir.protocol()==""){ dir = Springy::Util::Uri(std::string("file://")+directory); }
                  this->config->volumes.removeVolume(dir, vmountpoint);
              }catch(...){
                  errno = EINVAL;
              }
          }
          break;
      }

      if(errno!=0){
          ss << "{success: false, message: \"processing directory " << directory << " failed with " << errno << ":" << strerror(errno) << "\", data: null}";
      }
      else{
          ss << "{success: true, message: \"directory successfully processed\", data: null}";
      }
  }while(0);
  
  std::string response = ss.str();

  this->sendResponse(response, nc, hm);
}
void Httpd::list_directory(struct mg_connection *nc, struct http_message *hm){
    errno = 0;

    Springy::Volumes::VolumesMap vols = this->config->volumes.getVolumes();
    Springy::Volumes::VolumesMap::iterator it;
    std::stringstream ss;

    ss << "{success: false, message: \"volumes loaded\", data:";
    ss << "[";
    for(it=vols.begin();it!=vols.end();it++){
        boost::filesystem::path virtualMountPoint = it->first;
        for(size_t i=0;i<it->second.size();i++){
            ss << "{ volume: \"" << it->second[i]->string() << "\", virtualmountpoint: \"" << virtualMountPoint.string() << "\" }";
        }
    }
    ss << "]";
    ss << "}";
    std::string response = ss.str();

    this->sendResponse(response, nc, hm);
}

///// VOLUME API //////

void Httpd::fs_getattr(struct mg_connection *nc, struct http_message *hm){
    nlohmann::json j = nlohmann::json::parse(std::string(hm->body.p, hm->body.len));
    std::string path = j["path"];
    boost::filesystem::path p(path);

    // call getattr
    struct stat st;
    Springy::FsOps::Abstract::MetaRequest meta;
    j["errno"] = this->operations->getattr(meta, p, &st);
    if(j["errno"]==0){
        j["st_dev"]     = st.st_dev;
        j["st_ino"]     = st.st_ino;
        j["st_mode"]    = st.st_mode;
        j["st_nlink"]   = st.st_nlink;
        j["st_uid"]     = st.st_uid;
        j["st_gid"]     = st.st_gid;
        j["st_rdev"]    = st.st_rdev;
        j["st_size"]    = st.st_size;
        j["st_blksize"] = st.st_blksize;
        j["st_blocks"]  = st.st_blocks;
        j["st_atime"]   = st.st_atime;
        j["st_mtime"]   = st.st_mtime;
        j["st_ctime"]   = st.st_ctime;
    }

    std::string response = j.dump();
    this->sendResponse(response, nc, hm);
}

void Httpd::fs_statfs(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_readdir(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_readlink(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_access(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_mkdir(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_rmdir(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_unlink(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_rename(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_utimens(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_chmod(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_chown(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_symlink(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_link(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_mknod(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_setxattr(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_getxattr(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_listxattr(struct mg_connection *nc, struct http_message *hm){}
void Httpd::fs_removexattr(struct mg_connection *nc, struct http_message *hm){}

///////////////////////////////////

void Httpd::handle_invalid_request(struct mg_connection *nc, struct http_message *hm){
    std::stringstream ss;

    ss << "{success: false, message: \"unkown request\", data: null}";
  
    std::string response = ss.str();
    this->sendResponse(response, nc, hm);
}

void Httpd::ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
    // maybe alternative: https://mongoose.googlecode.com/svn/trunk/examples/example.c
    // e.g. mg_set_uri_callback(ctx, "/api/fs/getattr", &show_post, NULL);
    Httpd *instance = (Httpd*)nc->user_data;
    struct http_message *hm = (struct http_message *) ev_data;

    switch (ev) {
        case MG_EV_HTTP_REQUEST:
        {
            std::string method(hm->method.p, hm->method.len);
            if(method == "OPTIONS"){
                struct mg_str *acrh = mg_get_http_header(hm, "Access-Control-Request-Headers");
                std::string accessControlRequestHeader;
                if(acrh){ accessControlRequestHeader=std::string(acrh->p, acrh->len); }

                mg_printf(nc, "HTTP/1.1 200 OK\r\n");
                mg_printf(nc, "Access-Control-Allow-Origin: *\r\n");
                mg_printf(nc, "Access-Control-Allow-Methods: POST, GET, OPTIONS, HEAD, PUT, DELETE\r\n");
                if(accessControlRequestHeader.length()>0){
                    mg_printf(nc, "Access-Control-Allow-Headers: %s\r\n", accessControlRequestHeader.c_str());
                }

                mg_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */

                break;
            }

            /*
             * 
                virtual int getattr(MetaRequest meta, const boost::filesystem::path file_name, struct stat *buf);
                virtual int truncate(MetaRequest meta, const boost::filesystem::path path, off_t size);
                virtual int statfs(MetaRequest meta, const boost::filesystem::path path, struct statvfs *buf);
                virtual int readdir(MetaRequest meta, const boost::filesystem::path dirname, void *buf, off_t offset, std::unordered_map<std::string, struct stat> &directories);
                virtual int readlink(MetaRequest meta, const boost::filesystem::path path, char *buf, size_t size);
                virtual int access(MetaRequest meta, const boost::filesystem::path path, int mask);
                virtual int mkdir(MetaRequest meta, const boost::filesystem::path path, mode_t mode);
                virtual int rmdir(MetaRequest meta, const boost::filesystem::path path);
                virtual int unlink(MetaRequest meta, const boost::filesystem::path path);
             * 
                virtual int rename(MetaRequest meta, const boost::filesystem::path from, const boost::filesystem::path to);
                virtual int utimens(MetaRequest meta, const boost::filesystem::path path, const struct timespec ts[2]);
                virtual int chmod(MetaRequest meta, const boost::filesystem::path path, mode_t mode);
                virtual int chown(MetaRequest meta, const boost::filesystem::path path, uid_t uid, gid_t gid);
                virtual int symlink(MetaRequest meta, const boost::filesystem::path from, const boost::filesystem::path to);
                virtual int link(MetaRequest meta, const boost::filesystem::path from, const boost::filesystem::path to);
                virtual int mknod(MetaRequest meta, const boost::filesystem::path path, mode_t mode, dev_t rdev);

                virtual int setxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname,
                                     const char *attrval, size_t attrvalsize, int flags);
                virtual int getxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname, char *buf, size_t count);
                virtual int listxattr(MetaRequest meta, const boost::filesystem::path file_name, char *buf, size_t count);
                virtual int removexattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname);

             */
            
            if (mg_vcmp(&hm->uri, "/api/addDirectory") == 0) {
                instance->handle_directory(1, nc, hm);
            } else if (mg_vcmp(&hm->uri, "/api/remDirectory") == 0) {
                instance->handle_directory(0, nc, hm);
            } else if (mg_vcmp(&hm->uri, "/api/listDirectory") == 0) {
                instance->list_directory(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/getattr") == 0){
                instance->fs_getattr(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/statfs") == 0){
                instance->fs_statfs(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/readdir") == 0){
                instance->fs_readdir(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/readlink") == 0){
                instance->fs_readlink(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/access") == 0){
                instance->fs_access(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/mkdir") == 0){
                instance->fs_mkdir(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/rmdir") == 0){
                instance->fs_rmdir(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/unlink") == 0){
                instance->fs_unlink(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/rename") == 0){
                instance->fs_rename(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/utimens") == 0){
                instance->fs_utimens(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/chmod") == 0){
                instance->fs_chmod(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/chown") == 0){
                instance->fs_chown(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/symlink") == 0){
                instance->fs_symlink(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/link") == 0){
                instance->fs_link(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/mknod") == 0){
                instance->fs_mknod(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/setxattr") == 0){
                instance->fs_setxattr(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/getxattr") == 0){
                instance->fs_getxattr(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/listxattr") == 0){
                instance->fs_listxattr(nc, hm);
            } else if(mg_vcmp(&hm->uri, "/api/fs/removexattr") == 0){
                instance->fs_removexattr(nc, hm);
            } else {
                instance->handle_invalid_request(nc, hm);
            }
        }
        break;
      default:
        break;
    }
}

/* this function is run by the second thread */
void* Httpd::server(void *arg)
{
    Httpd* instance = (Httpd*)arg;

/*
    int res = 0, fd;
    struct ifreq ifr;
    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_port = htons(mhdd->server_port);
    if(mhdd->server_iface!=NULL && inet_pton(AF_INET, mhdd->server_iface, &address.sin_addr)==0){
         fd = socket(AF_INET, SOCK_DGRAM, 0);
         ifr.ifr_addr.sa_family = AF_INET;
         strncpy(ifr.ifr_name, mhdd->server_iface, IFNAMSIZ-1);
         res = ioctl(fd, SIOCGIFADDR, &ifr);
         close(fd);
         if(res==-1){
             return NULL;
         }

         address.sin_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
    }
*/

    while(true) {
        mg_mgr_poll(&instance->data.mgr, 500);

        {
            Synchronized sync(instance->data.shall_run, Synchronized::LockType::READ);
            if(!instance->data.shall_run){
                break;
            }
        }
    }

    mg_mgr_free(&instance->data.mgr);

    return NULL;
}

}
