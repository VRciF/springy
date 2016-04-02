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
#include "fsops/local.hpp"

#include "util/json.hpp"

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
            Springy::Settings *config;
            Springy::LibC::ILibC *libc;
            
            Springy::FsOps::Local *operations;
            
            std::multimap<std::string, int> mapRemoteHostToFD;

            void sendResponse(std::string response, struct mg_connection *nc, struct http_message *hm);
            Springy::FsOps::Abstract::MetaRequest getMetaFromJson(nlohmann::json j);
            
            void closeOpenFilesByConnection(struct mg_connection *nc);

        public:
            void handle_directory(int what, struct mg_connection *nc, struct http_message *hm);
            void list_directory(struct mg_connection *nc, struct http_message *hm);
            
            void fs_getattr(struct mg_connection *nc, struct http_message *hm);
            void fs_statfs(struct mg_connection *nc, struct http_message *hm);
            void fs_readdir(struct mg_connection *nc, struct http_message *hm);
            void fs_readlink(struct mg_connection *nc, struct http_message *hm);
            void fs_access(struct mg_connection *nc, struct http_message *hm);
            void fs_mkdir(struct mg_connection *nc, struct http_message *hm);
            void fs_rmdir(struct mg_connection *nc, struct http_message *hm);
            void fs_unlink(struct mg_connection *nc, struct http_message *hm);
            void fs_rename(struct mg_connection *nc, struct http_message *hm);
            void fs_utimens(struct mg_connection *nc, struct http_message *hm);
            void fs_chmod(struct mg_connection *nc, struct http_message *hm);
            void fs_chown(struct mg_connection *nc, struct http_message *hm);
            void fs_symlink(struct mg_connection *nc, struct http_message *hm);
            void fs_link(struct mg_connection *nc, struct http_message *hm);
            void fs_mknod(struct mg_connection *nc, struct http_message *hm);
            void fs_setxattr(struct mg_connection *nc, struct http_message *hm);
            void fs_getxattr(struct mg_connection *nc, struct http_message *hm);
            void fs_listxattr(struct mg_connection *nc, struct http_message *hm);
            void fs_removexattr(struct mg_connection *nc, struct http_message *hm);

            void fs_truncate(struct mg_connection *nc, struct http_message *hm);
            void fs_create(struct mg_connection *nc, struct http_message *hm);
            void fs_open(struct mg_connection *nc, struct http_message *hm);
            void fs_release(struct mg_connection *nc, struct http_message *hm);
            void fs_read(struct mg_connection *nc, struct http_message *hm);
            void fs_write(struct mg_connection *nc, struct http_message *hm);
            void fs_fsync(struct mg_connection *nc, struct http_message *hm);

            void handle_invalid_request(struct mg_connection *nc, struct http_message *hm);

            static void ev_handler(struct mg_connection *nc, int ev, void *ev_data);
            static void* server(void *arg);

            Httpd();
            Httpd& init(Settings *config, Springy::LibC::ILibC *libc);
            void start();
            void stop();
            ~Httpd();
    };
}


#endif
