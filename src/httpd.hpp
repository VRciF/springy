#ifndef SPRINGY_HTTPD_H
#define SPRINGY_HTTPD_H

#include <pthread.h>

extern "C"
{
#ifndef __DARWIN_C_LEVEL
#define __DARWIN_C_LEVEL_TMP
#define __DARWIN_C_LEVEL 200809L
#endif
    
#include "mongoose.h"

#ifdef __DARWIN_C_LEVEL_TMP
#undef __DARWIN_C_LEVEL
#endif
}
#include "settings.hpp"

namespace Springy{
    class Httpd{
        protected:
            bool running;
            typedef struct http_meta{
                pthread_t thHttpServer;

                struct mg_mgr mgr;
                struct mg_connection *serverSocket;
                struct mg_serve_http_opts s_http_server_opts;

                bool shall_run;
            } http_meta;

            http_meta data;
            Settings *config;

        public:
            void handle_directory(int what, struct mg_connection *nc, struct http_message *hm);
            void list_directory(struct mg_connection *nc, struct http_message *hm);
            
            void volume_getattr(struct mg_connection *nc, struct http_message *hm);
            
            void handle_invalid_request(struct mg_connection *nc, struct http_message *hm);

            static void ev_handler(struct mg_connection *nc, int ev, void *ev_data);
            static void* server(void *arg);

            Httpd();
            Httpd& init(Settings *config);
            void start();
            void stop();
            ~Httpd();
    };
}


#endif
