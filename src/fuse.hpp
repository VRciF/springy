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

            std::string concatPath(const std::string &p1, const std::string &p2);            
            std::string findPath(std::string file_name, struct stat *buf=NULL);

            void* op_init(struct fuse_conn_info *conn);
            void op_destroy(void *arg);
            int op_getattr(const std::string file_name, struct stat *buf);
            int op_statfs(const std::string path, struct statvfs *buf);
            int op_readdir(const std::string dirname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi);
			int op_readlink(const std::string path, char *buf, size_t size);
            int op_setxattr(const std::string file_name, const std::string attrname,
							    const char *attrval, size_t attrvalsize, int flags);
			int op_getxattr(const std::string file_name, const std::string attrname, char *buf, size_t count);
			int op_listxattr(const std::string file_name, char *buf, size_t count);
			int op_removexattr(const std::string file_name, const std::string attrname);


            static void* init(struct fuse_conn_info *conn);
            static void destroy(void *arg);
            static int getattr(const char *file_name, struct stat *buf);
            static int statfs(const char *path, struct statvfs *buf);
            static int readdir(const char *dirname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi);
            static int readlink(const char *path, char *buf, size_t size);
            static int setxattr(const char *path, const char *attrname,
							    const char *attrval, size_t attrvalsize, int flags);
			static int getxattr(const char *path, const char *attrname, char *buf, size_t count);
			static int listxattr(const char *path, char *buf, size_t count);
			static int removexattr(const char *path, const char *attrname);

        protected:
            Settings *config;
            bool singleThreaded;
            static const char *fsname;
            std::string mountpoint;
            char *fmountpoint;
            std::string fuseoptions;
            std::thread th;
            
            struct openFile{
				std::string *path;
				int descriptor;
			};
			std::unordered_map<std::string, struct openFile*> m_openFilesByPath;
			std::unordered_map<int, struct openFile*> m_openFilesByDescriptor;

            struct fuse_operations fops;
            struct fuse* fuse;
    };

}


#endif

#endif
