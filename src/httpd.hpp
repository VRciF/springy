#ifndef SPRINGY_HTTPD_H
#define SPRINGY_HTTPD_H

#include <pthread.h>

#include "fossa.h"
#include "settings.hpp"

namespace Springy{
    class Httpd{
        protected:
            bool running;
            typedef struct http_meta{
                pthread_t thHttpServer;

                struct ns_mgr mgr;
                struct ns_connection *serverSocket;
                struct ns_serve_http_opts s_http_server_opts;

                bool shall_run;
            } http_meta;

            http_meta data;
            Settings *config;

        public:
            void handle_directory(int what, struct ns_connection *nc, struct http_message *hm);
            void list_directory(struct ns_connection *nc, struct http_message *hm);
            void handle_invalid_request(struct ns_connection *nc, struct http_message *hm);

            static void ev_handler(struct ns_connection *nc, int ev, void *ev_data);
            static void* server(void *arg);

            Httpd();
            Httpd& init(Settings *config);
            void start();
            void stop();
            ~Httpd();
    };
}


#endif
