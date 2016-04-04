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

            void handle_directory(int what, struct mg_connection *nc, struct http_message *hm);
            void list_directory(struct mg_connection *nc, struct http_message *hm);

            nlohmann::json routeRequest(std::string uri, std::string remotehost, nlohmann::json j);

            nlohmann::json fs_getattr(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_statfs(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_readdir(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_readlink(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_access(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_mkdir(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_rmdir(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_unlink(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_rename(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_utimens(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_chmod(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_chown(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_symlink(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_link(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_mknod(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_setxattr(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_getxattr(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_listxattr(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_removexattr(std::string remotehost, nlohmann::json j);

            nlohmann::json fs_truncate(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_create(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_open(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_release(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_read(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_write(std::string remotehost, nlohmann::json j);
            nlohmann::json fs_fsync(std::string remotehost, nlohmann::json j);

            void handle_invalid_request(struct mg_connection *nc, struct http_message *hm);

        public:

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
