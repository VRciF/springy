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
#include "fsops/fuse.hpp"

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

            Springy::FsOps::Fuse *operations;
            
            
            bool readonly;
            bool singleThreaded;
            bool withinTearDown;

            static const char *fsname;
            boost::filesystem::path mountpoint;
            char *fmountpoint;
            std::string fuseoptions;
            std::thread th;

            struct of_idx_volumeFile{};
            struct of_idx_fd{};
            struct openFile{
                boost::filesystem::path volumeFile;

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
                boost::multi_index::ordered_non_unique<boost::multi_index::tag<of_idx_volumeFile>,boost::multi_index::member<openFile,boost::filesystem::path,&openFile::volumeFile> >
              > 
            > openFiles_set;

            openFiles_set openFiles;

            struct fuse_operations fops;
            struct fuse* fuse;

            void saveFd(boost::filesystem::path file, Springy::Volume::IVolume *volume, int fd, int flags);

            int copy_xattrs(Springy::Volume::IVolume *src, Springy::Volume::IVolume *dst, const boost::filesystem::path path);

            void reopen_files(const boost::filesystem::path file, const Springy::Volume::IVolume *volume);
            void move_file(int fd, boost::filesystem::path file, Springy::Volume::IVolume *from, fsblkcnt_t wsize);

            void determineCaller(uid_t *u=NULL, gid_t *g=NULL, pid_t *p=NULL, mode_t *mask=NULL);
    };

}


#endif

#endif
