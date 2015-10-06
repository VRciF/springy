#ifndef __SPRINGY_FUSE_HPP__
#define __SPRINGY_FUSE_HPP__

#ifdef HAS_FUSE

#include <fuse.h>

#include <thread>

#include "settings.hpp"

namespace Springy{
    class Fuse{
        protected:
            enum fop{
                INIT, DESTROY, GETATTR
            };
        public:
            Fuse();
            Fuse& init(Settings *config);
            Fuse& setUp(bool singleThreaded);
            Fuse& run();
            Fuse& tearDown();

            static void thread(Fuse *instance);
            void determineCaller(uid_t *u, gid_t *g, pid_t *p, mode_t *mask);

            ~Fuse();

            void* op_init(struct fuse_conn_info *conn);
            void op_destroy(void *arg);
            int op_getattr(const std::string file_name, struct stat *buf);

            static void* init(struct fuse_conn_info *conn);
            static void destroy(void *arg);
            static int getattr(const char *file_name, struct stat *buf);

        protected:
            Settings *config;
            bool singleThreaded;
            static const char *fsname;
            std::string mountpoint;
            char *fmountpoint;
            std::string fuseoptions;
            std::thread th;

            struct fuse_operations fops;
            struct fuse* fuse;
    };

}


#endif

#endif
