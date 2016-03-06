#ifndef __SPRINGY_FUSE_HPP__
#define __SPRINGY_FUSE_HPP__

#ifdef HAS_FUSE

#include <fuse.h>

#include <thread>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <boost/filesystem.hpp>

#include "settings.hpp"

#include <map>

#include "libc/ilibc.hpp"

namespace Springy{
    class Fuse{
        protected:
            enum fop{
                INIT, DESTROY, GETATTR
            };
        public:
            Fuse();
            Fuse& init(Springy::Settings *config, Springy::LibC::ILibC *libc);
            Fuse& setUp(bool singleThreaded);
            Fuse& run();
            Fuse& tearDown();

            static void thread(Fuse *instance);

            ~Fuse();

            void* op_init(struct fuse_conn_info *conn);
            void op_destroy(void *arg);
            int op_getattr(const boost::filesystem::path file_name, struct stat *buf);
            int op_statfs(const boost::filesystem::path path, struct statvfs *buf);
            int op_readdir(const boost::filesystem::path dirname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi);
			int op_readlink(const boost::filesystem::path path, char *buf, size_t size);
            int op_create(const boost::filesystem::path file, mode_t mode, struct fuse_file_info *fi);
            int op_open(const boost::filesystem::path file, struct fuse_file_info *fi);
            int op_release(const boost::filesystem::path path, struct fuse_file_info *fi);
            int op_read(const boost::filesystem::path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi);
            int op_write(const boost::filesystem::path file, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi);
            int op_truncate(const boost::filesystem::path path, off_t size);
            int op_ftruncate(const boost::filesystem::path path, off_t size, struct fuse_file_info *fi);
            int op_access(const boost::filesystem::path path, int mask);
            int op_mkdir(const boost::filesystem::path path, mode_t mode);
            int op_rmdir(const boost::filesystem::path path);
            int op_unlink(const boost::filesystem::path path);
            int op_rename(const boost::filesystem::path from, const boost::filesystem::path to);
            int op_utimens(const boost::filesystem::path path, const struct timespec ts[2]);
            int op_chmod(const boost::filesystem::path path, mode_t mode);
            int op_chown(const boost::filesystem::path path, uid_t uid, gid_t gid);
            int op_symlink(const boost::filesystem::path from, const boost::filesystem::path to);
            int op_link(const boost::filesystem::path from, const boost::filesystem::path to);
            int op_mknod(const boost::filesystem::path path, mode_t mode, dev_t rdev);
            int op_fsync(const boost::filesystem::path path, int isdatasync, struct fuse_file_info *fi);
            int op_lock(const boost::filesystem::path path, struct fuse_file_info *fi, int cmd, struct flock *lck);
            //int op_setxattr(const boost::filesystem::path file_name, const std::string attrname,
			//				    const char *attrval, size_t attrvalsize, int flags);
			//int op_getxattr(const boost::filesystem::path file_name, const std::string attrname, char *buf, size_t count);
			//int op_listxattr(const boost::filesystem::path file_name, char *buf, size_t count);
			//int op_removexattr(const boost::filesystem::path file_name, const std::string attrname);


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
            static int lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lck);
            static int setxattr(const char *path, const char *attrname,
							    const char *attrval, size_t attrvalsize, int flags);
			static int getxattr(const char *path, const char *attrname, char *buf, size_t count);
			static int listxattr(const char *path, char *buf, size_t count);
			static int removexattr(const char *path, const char *attrname);

        protected:
            Springy::Settings *config;
            Springy::LibC::ILibC *libc;
            
            bool readonly;

            bool singleThreaded;
            static const char *fsname;
            boost::filesystem::path mountpoint;
            char *fmountpoint;
            std::string fuseoptions;
            std::thread th;

            struct of_idx_fuseFile{};
            struct of_idx_fd{};
            struct openFile{
                boost::filesystem::path fuseFile;

                Springy::Volume::IVolume *volume;

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
                boost::multi_index::ordered_non_unique<boost::multi_index::tag<of_idx_fuseFile>,boost::multi_index::member<openFile,boost::filesystem::path,&openFile::fuseFile> >
              > 
            > openFiles_set;

            openFiles_set openFiles;

            struct fuse_operations fops;
            struct fuse* fuse;

            void saveFd(boost::filesystem::path file, Springy::Volume::IVolume *volume, int fd, int flags);
            boost::filesystem::path concatPath(const boost::filesystem::path &p1, const boost::filesystem::path &p2);

            Springy::Volume::IVolume* findVolume(const boost::filesystem::path file_name, struct stat *buf = NULL);
            Springy::Volume::IVolume* getMaxFreeSpaceVolume(const boost::filesystem::path path, uintmax_t *space = NULL);

            boost::filesystem::path get_parent_path(const boost::filesystem::path path);
            boost::filesystem::path get_base_name(const boost::filesystem::path path);
            int cloneParentDirsIntoVolume(Springy::Volume::IVolume *volume, const boost::filesystem::path path);
            int copy_xattrs(Springy::Volume::IVolume *src, Springy::Volume::IVolume *dst, const boost::filesystem::path path);

            int dir_is_empty(const boost::filesystem::path path);
            void reopen_files(const boost::filesystem::path file, const Springy::Volume::IVolume *volume);
            void move_file(int fd, boost::filesystem::path file, Springy::Volume::IVolume *from, fsblkcnt_t wsize);

            void determineCaller(uid_t *u=NULL, gid_t *g=NULL, pid_t *p=NULL, mode_t *mask=NULL);
    };

}


#endif

#endif
