#include <stdlib.h>
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
#include <stdexcept>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#include "httpd.hpp"
#include "exception.hpp"

namespace Springy{

Httpd::Httpd(){
    this->running = false;
}
Httpd& Httpd::init(Settings *config){
    this->config = config;
    return *this;
}
void Httpd::start(){
    if(this->running){ return; }

    memset(&this->data, '\0', sizeof(this->data));
    this->data.shall_run = true;

    int server_port = 8787;
    if(this->config->httpdPort != 0){
        server_port = this->config->httpdPort;
    }
    std::cout << "server port: " << server_port << std::endl;

    char serveraddr[64] = {'\0'};
    //snprintf(serveraddr, sizeof(serveraddr)-1, "0.0.0.0:%d", server_port);
    snprintf(serveraddr, sizeof(serveraddr)-1, "%d", server_port);

    ns_mgr_init(&this->data.mgr, NULL);
    ns_bind_opts opts;
    memset(&opts, '\0', sizeof(opts));
    opts.user_data = (void*)this;
    this->data.serverSocket = ns_bind_opt(&this->data.mgr, serveraddr, Httpd::ev_handler, opts);
    if(this->data.serverSocket==NULL){
        ns_mgr_free(&this->data.mgr);
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "binding to port failed: " << serveraddr;
    }
    ns_set_protocol_http_websocket(this->data.serverSocket);

    this->data.s_http_server_opts.document_root = NULL;

#ifdef NS_ENABLE_SSL
    if(mhdd->server_cert_pem!=NULL){
      const char *err_str = ns_set_ssl(nc, ssl_cert, NULL);
      if (err_str != NULL) {
        ns_mgr_free(&this->data.mgr);
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
        ns_mgr_free(&this->data.mgr);
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




void Httpd::handle_directory(int what, struct ns_connection *nc, struct http_message *hm){
    std::string contentType("application/json");
  char directory[8192] = {'\0'};
  char vmountpoint[8192] = {'\0'};
  char callback[256]   = {'\0'};

  std::stringstream ss;
  
  struct ns_str *acrh = ns_get_http_header(hm, "Access-Control-Request-Headers");
  std::string accessControlRequestHeader;
  if(acrh){ accessControlRequestHeader=std::string(acrh->p, acrh->len); }

  /* Get form variables */
  do{
      ns_get_http_var(&hm->query_string, "callback", callback, sizeof(callback));
      ns_get_http_var(&hm->query_string, "vmountpoint", vmountpoint, sizeof(vmountpoint));
      if(strlen(vmountpoint)<=0){
          vmountpoint[0] = '/';
      }
      else if(vmountpoint[0] != '/'){
          ss << "{success: false, message: \"given vmountpoint parameter must start with a slash(/)-character\", data: null}";
          break;
      }

      if(ns_get_http_var(&hm->query_string, "directory", directory, sizeof(directory))<=0){
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
  if(strlen(callback)>0){
      response = std::string(callback)+"("+response+");";
      contentType = "application/javascript";
  }

  /* Send headers */
  ns_printf(nc, "HTTP/1.1 200 OK\r\n");
  ns_printf(nc, "Content-Length: %d\r\n", response.length());
  ns_printf(nc, "Content-Type: %s\r\n", contentType.c_str());
  ns_printf(nc, "Access-Control-Allow-Origin: *\r\n");
  ns_printf(nc, "Access-Control-Allow-Methods: POST, GET, OPTIONS, HEAD, PUT, DELETE\r\n");
  if(accessControlRequestHeader.length()>0){
      ns_printf(nc, "Access-Control-Allow-Headers: %s\r\n", accessControlRequestHeader.c_str());
  }
  ns_printf(nc, "Access-Control-Max-Age: 1728000\r\n");
  ns_printf(nc, "Transfer-Encoding: chunked\r\n\r\n");

  ns_printf_http_chunk(nc, response.c_str());

  ns_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */
}
void Httpd::list_directory(struct ns_connection *nc, struct http_message *hm){
    std::string contentType("application/json");
    char callback[256]   = {'\0'};

    struct ns_str *acrh = ns_get_http_header(hm, "Access-Control-Request-Headers");
    std::string accessControlRequestHeader;
    if(acrh){ accessControlRequestHeader=std::string(acrh->p, acrh->len); }

    /* Get form variables */

    ns_get_http_var(&hm->query_string, "callback", callback, sizeof(callback));

    errno = 0;

    std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > vols = this->config->volumes.getVolumes();
    std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> >::iterator it;
    std::stringstream ss;

    ss << "{success: false, message: \"volumes loaded\", data:";
    ss << "[";
    for(it=vols.begin();it!=vols.end();it++){
        ss << "{ volume: \"" << it->first->string() << "\", virtualmountpoint: \"" << it->second.string() << "\" }";
    }
    ss << "]";
    ss << "}";
    std::string response = ss.str();
    if(strlen(callback)>0){
        response = std::string(callback)+"("+response+");";
        contentType = "application/javascript";
    }

    /* Send headers */
    ns_printf(nc, "HTTP/1.1 200 OK\r\n");
    ns_printf(nc, "Content-Length: %d\r\n", response.length());
    ns_printf(nc, "Content-Type: %s\r\n", contentType.c_str());
    ns_printf(nc, "Access-Control-Allow-Origin: *\r\n");
    ns_printf(nc, "Access-Control-Allow-Methods: POST, GET, OPTIONS, HEAD, PUT, DELETE\r\n");
    if(accessControlRequestHeader.length()>0){
        ns_printf(nc, "Access-Control-Allow-Headers: %s\r\n", accessControlRequestHeader.c_str());
    }
    ns_printf(nc, "Access-Control-Max-Age: 1728000\r\n");
    ns_printf(nc, "Transfer-Encoding: chunked\r\n\r\n");

    ns_printf_http_chunk(nc, response.c_str());

    ns_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */
}

void Httpd::handle_invalid_request(struct ns_connection *nc, struct http_message *hm){
    std::string contentType("application/json");
    char callback[256]   = {'\0'};

    struct ns_str *acrh = ns_get_http_header(hm, "Access-Control-Request-Headers");
    std::string accessControlRequestHeader;
    if(acrh){ accessControlRequestHeader=std::string(acrh->p, acrh->len); }

    /* Get form variables */

    ns_get_http_var(&hm->query_string, "callback", callback, sizeof(callback));

    std::stringstream ss;

    ss << "{success: false, message: \"unkown request\", data: null}";
  
    std::string response = ss.str();
    if(strlen(callback)>0){
        response = std::string(callback)+"("+response+");";
        contentType = "application/javascript";
    }

    /* Send headers */
    ns_printf(nc, "HTTP/1.1 200 OK\r\n");
    ns_printf(nc, "Content-Length: %d\r\n", response.length());
    ns_printf(nc, "Content-Type: %s\r\n", contentType.c_str());
    ns_printf(nc, "Access-Control-Allow-Origin: *\r\n");
    ns_printf(nc, "Access-Control-Allow-Methods: POST, GET, OPTIONS, HEAD, PUT, DELETE\r\n");
    if(accessControlRequestHeader.length()>0){
        ns_printf(nc, "Access-Control-Allow-Headers: %s\r\n", accessControlRequestHeader.c_str());
    }
    ns_printf(nc, "Access-Control-Max-Age: 1728000\r\n");
    ns_printf(nc, "Transfer-Encoding: chunked\r\n\r\n");

    ns_printf_http_chunk(nc, response.c_str());

    ns_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */
}

void Httpd::ev_handler(struct ns_connection *nc, int ev, void *ev_data) {
    Httpd *instance = (Httpd*)nc->user_data;
    struct http_message *hm = (struct http_message *) ev_data;

    switch (ev) {
      case NS_HTTP_REQUEST:
        if (ns_vcmp(&hm->uri, "/api/addDirectory") == 0) {
            instance->handle_directory(1, nc, hm);
        } else if (ns_vcmp(&hm->uri, "/api/remDirectory") == 0) {
            instance->handle_directory(0, nc, hm);
        } else if (ns_vcmp(&hm->uri, "/api/listDirectory") == 0) {
            instance->list_directory(nc, hm);
        } else {
            instance->handle_invalid_request(nc, hm);
          //ns_serve_http(nc, hm, instance->data.s_http_server_opts);
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
        ns_mgr_poll(&instance->data.mgr, 500);

        {
            Synchronized sync(instance->data.shall_run, Synchronized::LockType::READ);
            if(!instance->data.shall_run){
                break;
            }
        }
    }

    ns_mgr_free(&instance->data.mgr);

    return NULL;
}

}
