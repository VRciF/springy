#ifndef __SPRINGY_FUSE_HPP__
#define __SPRINGY_FUSE_HPP__

#ifdef HAS_FUSE

#include <fuse.h>

#include <thread>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

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
            std::string findPath(std::string file_name, struct stat *buf=NULL, std::string *usedPath=NULL);
            std::string getMaxFreeSpaceDir(fsblkcnt_t *space=NULL);
            std::string get_parent_path(const std::string path);
            std::string get_base_name(const std::string path);
            int create_parent_dirs(std::string dir, const std::string path);
            #ifndef WITHOUT_XATTR
            int copy_xattrs(const std::string from, const std::string to);
            #endif
            int dir_is_empty(const std::string path);
            void reopen_files(const std::string file, const std::string newDirectory);
            void move_file(int fd, std::string file, std::string directory, fsblkcnt_t wsize);

            int internal_open(const std::string file, mode_t mode, struct fuse_file_info *fi);

            void* op_init(struct fuse_conn_info *conn);
            void op_destroy(void *arg);
            int op_getattr(const std::string file_name, struct stat *buf);
            int op_statfs(const std::string path, struct statvfs *buf);
            int op_readdir(const std::string dirname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi);
			int op_readlink(const std::string path, char *buf, size_t size);
            int op_create(const std::string file, mode_t mode, struct fuse_file_info *fi);
            int op_open(const std::string file, struct fuse_file_info *fi);
            int op_release(const std::string path, struct fuse_file_info *fi);
            int op_read(const std::string, char *buf, size_t count, off_t offset, struct fuse_file_info *fi);
            int op_write(const std::string file, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi);
            int op_truncate(const std::string path, off_t size);
            int op_ftruncate(const std::string path, off_t size, struct fuse_file_info *fi);
            int op_access(const std::string path, int mask);
            int op_mkdir(const std::string path, mode_t mode);
            int op_rmdir(const std::string path);
            int op_unlink(const std::string path);
            int op_rename(const std::string from, const std::string to);
            int op_utimens(const std::string path, const struct timespec ts[2]);
            int op_chmod(const std::string path, mode_t mode);
            int op_chown(const std::string path, uid_t uid, gid_t gid);
            int op_symlink(const std::string from, const std::string to);
            int op_link(const std::string from, const std::string to);
            int op_mknod(const std::string path, mode_t mode, dev_t rdev);
            int op_fsync(const std::string path, int isdatasync, struct fuse_file_info *fi);

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
            static int create(const char *file, mode_t mode, struct fuse_file_info *fi);
            static int open(const char *file, struct fuse_file_info *fi);
            static int release(const char *path, struct fuse_file_info *fi);
            static int read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi);
            static int write(const char *file, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi);
            static int truncate(const char *path, off_t size);
            static int ftruncate(const char *path, off_t size, struct fuse_file_info *fi);
            static int access(const char *path, int mask);
            static int mkdir(const char *path, mode_t mode);
            static int rmdir(const char *path);
            static int unlink(const char *path);
            static int rename(const char *from, const char *to);
            static int utimens(const char *path, const struct timespec ts[2]);
            static int chmod(const char *path, mode_t mode);
            static int chown(const char *path, uid_t uid, gid_t gid);
            static int symlink(const char *from, const char *to);
            static int link(const char *from, const char *to);
            static int mknod(const char *path, mode_t mode, dev_t rdev);
            static int fsync(const char *path, int isdatasync, struct fuse_file_info *fi);
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

            struct of_idx_fuseFile{};
            struct of_idx_fd{};
            struct openFile{
                std::string fuseFile;

                mutable std::string path;

                int fd; // file descriptor is unique
                int flags;
                mutable int *syncToken;

                mutable bool valid;

                bool operator<(const openFile& o)const{return fd < o.fd; }
            };

            typedef boost::multi_index::multi_index_container<
              openFile,
              boost::multi_index::indexed_by<
                // sort by openFile::operator<
                boost::multi_index::ordered_unique<boost::multi_index::tag<of_idx_fd>, boost::multi_index::member<openFile,int,&openFile::fd> >,
                
                // sort by less<string> on name
                boost::multi_index::ordered_non_unique<boost::multi_index::tag<of_idx_fuseFile>,boost::multi_index::member<openFile,std::string,&openFile::fuseFile> >
              > 
            > openFiles_set;

            openFiles_set openFiles;

            struct fuse_operations fops;
            struct fuse* fuse;
    };

}


#endif

#endif
