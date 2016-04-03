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
#include "util/string.hpp"

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

void Httpd::closeOpenFilesByConnection(struct mg_connection *nc){
    char buf[64];
    mg_conn_addr_to_str(nc, buf, sizeof(buf), MG_SOCK_STRINGIFY_REMOTE|MG_SOCK_STRINGIFY_IP|MG_SOCK_STRINGIFY_PORT);
    std::string remoteHost(buf);

    std::pair<std::multimap<std::string, int>::iterator,
              std::multimap<std::string, int>::iterator> range = this->mapRemoteHostToFD.equal_range(remoteHost);
    for(;range.first!=range.second;range.first++){
        int fd = range.first->second;

        OpenFiles::openFile of = this->config->openFiles.getByDescriptor(fd);
        of.volume->close(of.volumeFile, of.fd);
        this->config->openFiles.remove(fd);
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
Springy::FsOps::Abstract::MetaRequest Httpd::getMetaFromJson(nlohmann::json j){
    Springy::FsOps::Abstract::MetaRequest meta;
    meta.g = j.value("group", -1);
    meta.u = j.value("user", -1);
    meta.mask = j.value("mask", -1);
    meta.readonly = j.value("readonly", -1);

    return meta;
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

nlohmann::json Httpd::fs_getattr(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    struct stat st;
    j.clear();
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

    return j;
}

nlohmann::json Httpd::fs_statfs(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    struct statvfs stvfs;
    j.clear();
    j["errno"] = this->operations->statfs(meta, p, &stvfs);
    if(j["errno"]==0){
        j["f_bsize"]    = stvfs.f_bsize;
        j["f_frsize"]   = stvfs.f_frsize;
        j["f_blocks"]   = stvfs.f_blocks;
        j["f_bfree"]    = stvfs.f_bfree;
        j["f_bavail"]   = stvfs.f_bavail;
        j["f_files"]    = stvfs.f_files;
        j["f_ffree"]    = stvfs.f_ffree;
        j["f_favail"]   = stvfs.f_favail;
        j["f_fsid"]     = stvfs.f_fsid;
        j["f_flag"]     = stvfs.f_flag;
        j["f_namemax"]  = stvfs.f_namemax;
    }

    return j;
}
nlohmann::json Httpd::fs_readdir(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    std::unordered_map<std::string, struct stat> directories;
    std::unordered_map<std::string, struct stat>::iterator dit;
    nlohmann::json jdirectories;
    j["errno"] = this->operations->readdir(meta, p, directories);
    if(j["errno"]==0){
        for(dit = directories.begin();dit!=directories.end();dit++){
            nlohmann::json entry;
            entry["st_dev"]     = dit->second.st_dev;
            entry["st_ino"]     = dit->second.st_ino;
            entry["st_mode"]    = dit->second.st_mode;
            entry["st_nlink"]   = dit->second.st_nlink;
            entry["st_uid"]     = dit->second.st_uid;
            entry["st_gid"]     = dit->second.st_gid;
            entry["st_rdev"]    = dit->second.st_rdev;
            entry["st_size"]    = dit->second.st_size;
            entry["st_blksize"] = dit->second.st_blksize;
            entry["st_blocks"]  = dit->second.st_blocks;
            entry["st_atime"]   = dit->second.st_atime;
            entry["st_mtime"]   = dit->second.st_mtime;
            entry["st_ctime"]   = dit->second.st_ctime;
            entry["path"]       = dit->first;

            jdirectories.push_back(entry);
        }
        j["directories"] = jdirectories;
    }

    return j;
}
nlohmann::json Httpd::fs_readlink(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    char buffer[2048];
    this->libc->memset(__LINE__, buffer, '\0', sizeof(buffer));
    j.clear();
    j["errno"] = this->operations->readlink(meta, p, buffer, sizeof(buffer));
    if(j["errno"]==0){
        j["link"] = std::string(buffer);
    }

    return j;
}
nlohmann::json Httpd::fs_access(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    int mode = j["mode"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->access(meta, p, mode);

    return j;
}
nlohmann::json Httpd::fs_mkdir(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    int mode = j["mode"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->mkdir(meta, p, mode);

    return j;
}
nlohmann::json Httpd::fs_rmdir(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->rmdir(meta, p);

    return j;
}
nlohmann::json Httpd::fs_unlink(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->unlink(meta, p);

    return j;
}
nlohmann::json Httpd::fs_rename(std::string remotehost, nlohmann::json j){
    std::string sfrom = j["old"];
    std::string sto   = j["new"];
    boost::filesystem::path from(sfrom);
    boost::filesystem::path to(sto);

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->rename(meta, from, to);

    return j;
}
nlohmann::json Httpd::fs_utimens(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    struct timespec times[2];
    times[0].tv_sec  = j["times"][0]["tv_sec"];
    times[0].tv_nsec = j["times"][0]["tv_nsec"];
    times[1].tv_sec  = j["times"][1]["tv_sec"];
    times[1].tv_nsec = j["times"][1]["tv_nsec"];

    j.clear();
    j["errno"] = this->operations->utimens(meta, p, times);

    return j;
}
nlohmann::json Httpd::fs_chmod(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    int mode = j["mode"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->chmod(meta, p, mode);

    return j;
}
nlohmann::json Httpd::fs_chown(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    uid_t u = j["owner"];
    gid_t g = j["group"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->chown(meta, p, u, g);

    return j;
}
nlohmann::json Httpd::fs_symlink(std::string remotehost, nlohmann::json j){
    std::string sfrom = j["old"];
    std::string sto   = j["new"];
    boost::filesystem::path from(sfrom);
    boost::filesystem::path to(sto);

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->symlink(meta, from, to);

    return j;
}
nlohmann::json Httpd::fs_link(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    std::string sfrom = j["old"];
    std::string sto   = j["new"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    boost::filesystem::path from(sfrom);
    boost::filesystem::path to(sto);
    j["errno"] = this->operations->link(meta, from, to);

    return j;
}
nlohmann::json Httpd::fs_mknod(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    int mode  = j["mode"];
    dev_t dev = j["dev"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->mknod(meta, p, mode, dev);

    return j;
}
nlohmann::json Httpd::fs_setxattr(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    std::string attrname  = j["xattr"];
    std::string value = ::Springy::Util::String::decode64(j["value"]);
    int flags = j["flags"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->setxattr(meta, p, attrname, value.data(), value.size(), flags);

    return j;
}
nlohmann::json Httpd::fs_getxattr(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    std::string attrname  = j["xattr"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();

    std::vector<char> buffer;
    int size = this->operations->getxattr(meta, p, attrname, NULL, 0);
    if(size > 0){
        buffer.resize(size+1);
        buffer[size] = '\0';
        size = this->operations->getxattr(meta, p, attrname, &buffer[0], size+1);
    }
    j["errno"] = size;
    if(size > 0){
        std::string value = std::string(&buffer[0], size);
        j["xattr"] = ::Springy::Util::String::encode64(value);
    }

    return j;
}
nlohmann::json Httpd::fs_listxattr(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();

    std::vector<char> buffer;
    int size = this->operations->listxattr(meta, p, NULL, 0);
    if(size > 0){
        buffer.resize(size+1);
        buffer[size] = '\0';
        size = this->operations->listxattr(meta, p, &buffer[0], size+1);
    }
    j["errno"] = size;
    if(size > 0){
        nlohmann::json jxattrs;
        std::string value;
        for(size_t i=0;i<buffer.size();i++){
            char ch = buffer[i];
            if(ch == '\0' && value.size()>0){
                value = ::Springy::Util::String::encode64(value);
                jxattrs.push_back(value);
                value = std::string();
                continue;
            }
            value.push_back(ch);
        }
        if(value.size()>0){
            value = ::Springy::Util::String::encode64(value);
            jxattrs.push_back(value);
        }
        j["xattrs"] = jxattrs;
    }

    return j;
}
nlohmann::json Httpd::fs_removexattr(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    std::string attrname  = j["xattr"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    j["errno"] = this->operations->link(meta, p, attrname);

    return j;
}

nlohmann::json Httpd::fs_truncate(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);

    // call getattr
    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    size_t size = j.value("size", -1);
    int fd = j.value("fd", -1);

    j.clear();

    if(fd >= 0){
        j["errno"] = this->operations->truncate(meta, p, size);
    }
    else{
        struct ::fuse_file_info fi;
        fi.fh = fd;
        j["errno"] = this->operations->ftruncate(meta, p, size, &fi);
    }

    return j;
}

nlohmann::json Httpd::fs_create(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    int flags   = j["flags"];
    mode_t mode = j["mode"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    struct ::fuse_file_info fi;
    fi.flags = flags;
    int fd = this->operations->create(meta, p, mode, &fi);
    if(fd < 0){
        j["errno"] = fd;
    }
    else{
        j["errno"] = 0;
        j["fd"] = fd;

        this->mapRemoteHostToFD.insert(std::make_pair(remotehost, fd));
    }

    return j;
}
nlohmann::json Httpd::fs_open(std::string remotehost, nlohmann::json j){
    std::string path = j["path"];
    boost::filesystem::path p(path);
    int flags   = j["flags"];

    Springy::FsOps::Abstract::MetaRequest meta = this->getMetaFromJson(j);

    j.clear();
    struct ::fuse_file_info fi;
    fi.flags = flags;
    int fd = this->operations->open(meta, p, &fi);
    if(fd < 0){
        j["errno"] = fd;
    }
    else{
        j["errno"] = 0;
        j["fd"] = fd;
        
        this->mapRemoteHostToFD.insert(std::make_pair(remotehost, fd));
    }

    return j;
}
nlohmann::json Httpd::fs_release(std::string remotehost, nlohmann::json j){
    return j;
}
nlohmann::json Httpd::fs_read(std::string remotehost, nlohmann::json j){ return j; }
nlohmann::json Httpd::fs_write(std::string remotehost, nlohmann::json j){ return j; }
nlohmann::json Httpd::fs_fsync(std::string remotehost, nlohmann::json j){ return j; }


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

    char buf[64];
    mg_conn_addr_to_str(nc, buf, sizeof(buf), MG_SOCK_STRINGIFY_REMOTE|MG_SOCK_STRINGIFY_IP|MG_SOCK_STRINGIFY_PORT);
    std::string remotehost(buf);

    switch (ev) {
        case MG_EV_WEBSOCKET_FRAME:
        {
            //struct websocket_message *wm = (struct websocket_message *) ev_data;
            
        }
        break;
        case MG_EV_HTTP_REQUEST:
        {
            struct http_message *hm = (struct http_message *) ev_data;

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

            std::string uri(hm->uri.p, hm->uri.len);
            
            bool handled = true;

            if (uri == "/api/addDirectory") {
                instance->handle_directory(1, nc, hm);
            } else if (uri == "/api/remDirectory") {
                instance->handle_directory(0, nc, hm);
            } else if (uri == "/api/listDirectory") {
                instance->list_directory(nc, hm);
            } else{
                nlohmann::json j = nlohmann::json::parse(std::string(hm->body.p, hm->body.len));

                if(uri == "/api/fs/getattr"){
                    j = instance->fs_getattr(remotehost, j);
                } else if(uri == "/api/fs/statfs"){
                    j = instance->fs_statfs(remotehost, j);
                } else if(uri == "/api/fs/readdir"){
                    j = instance->fs_readdir(remotehost, j);
                } else if(uri == "/api/fs/readlink"){
                    j = instance->fs_readlink(remotehost, j);
                } else if(uri == "/api/fs/access"){
                    j = instance->fs_access(remotehost, j);
                } else if(uri == "/api/fs/mkdir"){
                    j = instance->fs_mkdir(remotehost, j);
                } else if(uri == "/api/fs/rmdir"){
                    j = instance->fs_rmdir(remotehost, j);
                } else if(uri == "/api/fs/unlink"){
                    j = instance->fs_unlink(remotehost, j);
                } else if(uri == "/api/fs/rename"){
                    j = instance->fs_rename(remotehost, j);
                } else if(uri == "/api/fs/utimens"){
                    j = instance->fs_utimens(remotehost, j);
                } else if(uri == "/api/fs/chmod"){
                    j = instance->fs_chmod(remotehost, j);
                } else if(uri == "/api/fs/chown"){
                    j = instance->fs_chown(remotehost, j);
                } else if(uri == "/api/fs/symlink"){
                    j = instance->fs_symlink(remotehost, j);
                } else if(uri == "/api/fs/link"){
                    j = instance->fs_link(remotehost, j);
                } else if(uri == "/api/fs/mknod"){
                    j = instance->fs_mknod(remotehost, j);
                } else if(uri == "/api/fs/setxattr"){
                    j = instance->fs_setxattr(remotehost, j);
                } else if(uri == "/api/fs/getxattr"){
                    j = instance->fs_getxattr(remotehost, j);
                } else if(uri == "/api/fs/listxattr"){
                    j = instance->fs_listxattr(remotehost, j);
                } else if(uri == "/api/fs/removexattr"){
                    j = instance->fs_removexattr(remotehost, j);
                } else {
                    handled = false;
                }
                if(handled){
                    instance->sendResponse(std::string(j.dump()), nc, hm);
                }
            }
            
            if(!handled){
                instance->handle_invalid_request(nc, hm);
            }
        }
        break;
        case MG_EV_CLOSE:
        {
            instance->closeOpenFilesByConnection(nc);
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
