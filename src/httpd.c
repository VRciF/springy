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

#include "httpd.h"

#include "fossa.h"

#include "parse_options.h"
#include "tools.h"
#include "debug.h"

typedef struct mhddfs_http{
    pthread_t thHttpServer;

    struct ns_mgr mgr;
    struct ns_connection *serverSocket;
    struct ns_serve_http_opts s_http_server_opts;

    int shall_run;
}mhddfs_http;

static mhddfs_http mhddfs_http_instance;

static void mhddfs_http_handle_directory(int what, struct ns_connection *nc, struct http_message *hm){
  char directory[8192] = {'\0'};
  char callback[256] = {'\0'};
  char response[8192] = {'\0'};

  /* Get form variables */
  do{
      ns_get_http_var(&hm->query_string, "callback", callback, sizeof(callback));

      if(ns_get_http_var(&hm->query_string, "directory", directory, sizeof(directory))<=0){
          snprintf(response, sizeof(response)-1, "{success: false, message: 'invalid (length) directory parameter given', data: null}");
          break;
      }
      if(directory[0]!='/'){
          snprintf(response, sizeof(response)-1, "{success: false, message: 'invalid directory - only absolute paths starting with / are allowed', data: null}");
          break;
      }

      errno = 0;

      switch(what){
          case 1:
              add_mhdd_dirs(directory);
              break;
          case 0:
              rem_mhdd_dirs(directory);
              break;
      }

      if(errno!=0){
          snprintf(response, sizeof(response)-1, "{success: false, message: 'processing directory %s failed with %d:%s', data: null}", directory, errno, strerror(errno));
      }
      else{
          snprintf(response, sizeof(response)-1, "{success: true, message: 'directory successfully processed', data: null}");
      }
  }while(0);

  /* Send headers */
  ns_printf(nc, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nTransfer-Encoding: chunked\r\n\r\n", strlen(response));

  if(strlen(callback)>0){
      ns_printf_http_chunk(nc, callback);
      ns_printf_http_chunk(nc, "(");
      ns_printf_http_chunk(nc, response);
      ns_printf_http_chunk(nc, ");");
  }
  else{
      ns_printf_http_chunk(nc, response);
  }

  ns_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */
}

static void mhddfs_http_ev_handler(struct ns_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  switch (ev) {
    case NS_HTTP_REQUEST:
      if (ns_vcmp(&hm->uri, "/api/addDirectory") == 0) {
          mhddfs_http_handle_directory(1, nc, hm);
      } else if (ns_vcmp(&hm->uri, "/api/remDirectory") == 0) {
          mhddfs_http_handle_directory(0, nc, hm);
      } else {
        ns_serve_http(nc, hm, mhddfs_http_instance.s_http_server_opts);
      }
      break;
    default:
      break;
  }
}

/* this function is run by the second thread */
void* mhddfs_httpd_server(void *arg)
{
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

    while(mhddfs_http_instance.shall_run) {
        ns_mgr_poll(&mhddfs_http_instance.mgr, 500);
    }

    ns_mgr_free(&mhddfs_http_instance.mgr);

    return NULL;
}

int mhddfs_httpd_startServer(){
    memset(&mhddfs_http_instance, '\0', sizeof(mhddfs_http_instance));
    mhddfs_http_instance.shall_run = 1;

    char serveraddr[64] = {'\0'};
    snprintf(serveraddr, sizeof(serveraddr)-1, "0.0.0.0:%d", mhdd->server_port);

    ns_mgr_init(&mhddfs_http_instance.mgr, NULL);
    mhddfs_http_instance.serverSocket = ns_bind(&mhddfs_http_instance.mgr, serveraddr, mhddfs_http_ev_handler);
    if(mhddfs_http_instance.serverSocket==NULL){
        ns_mgr_free(&mhddfs_http_instance.mgr);
        return 0;
    }
    ns_set_protocol_http_websocket(mhddfs_http_instance.serverSocket);

    mhddfs_http_instance.s_http_server_opts.document_root = NULL;

#ifdef NS_ENABLE_SSL
    if(mhdd->server_cert_pem!=NULL){
      const char *err_str = ns_set_ssl(nc, ssl_cert, NULL);
      if (err_str != NULL) {
        fprintf(stderr, "Error loading SSL cert: %s\n", err_str);
        exit(1);
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


    if(pthread_create(&mhddfs_http_instance.thHttpServer, NULL, mhddfs_httpd_server, NULL)) {
        return 0;
    }
    mhdd_debug(MHDD_MSG, " >>>>> http server started on port %d \n", mhdd->server_port);
    return 1;
}
void mhddfs_httpd_stopServer(){
    mhddfs_http_instance.shall_run = 0;
}
