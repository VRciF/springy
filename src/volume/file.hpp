#ifndef SPRINGY_VOLUME_FILE
#define SPRINGY_VOLUME_FILE

#include "ivolume.hpp"
#include "../libc/ilibc.hpp"
#include "../util/uri.hpp"

namespace Springy{
    namespace Volume{
        class File : public Springy::Volume::IVolume{
            protected:
                Springy::LibC::ILibC *libc;
                Springy::Util::Uri u;
                bool readonly;

                boost::filesystem::path concatPath(const boost::filesystem::path &p1, const boost::filesystem::path &p2);

            public:
                File(Springy::LibC::ILibC *libc, Springy::Util::Uri u);
                virtual ~File();

                virtual std::string string();
                virtual bool isLocal();

                virtual int getattr(boost::filesystem::path v_file_name, struct stat *buf);

                virtual int statvfs(boost::filesystem::path v_path, struct ::statvfs *stat);

                virtual int chown(boost::filesystem::path v_file_name, uid_t owner, gid_t group);

                virtual int chmod(boost::filesystem::path v_file_name, mode_t mode);
                virtual int mkdir(boost::filesystem::path v_file_name, mode_t mode);
                virtual int rmdir(boost::filesystem::path v_path);

                virtual int rename(boost::filesystem::path v_old_name, boost::filesystem::path v_new_name);

                virtual int utimensat(boost::filesystem::path v_path, const struct timespec times[2]);

                virtual int readdir(boost::filesystem::path v_path, std::unordered_map<std::string, struct stat> &result);
                virtual ssize_t readlink(boost::filesystem::path v_path, char *buf, size_t bufsiz);

                virtual int open(boost::filesystem::path v_file_name, int flags, mode_t mode=0);
                virtual int creat(boost::filesystem::path v_file_name, mode_t mode);
                virtual int close(boost::filesystem::path v_file_name, int fd);

                virtual ssize_t write(boost::filesystem::path v_file_name, int fd, const void *buf, size_t count, off_t offset);
                virtual ssize_t read(boost::filesystem::path v_file_name, int fd, void *buf, size_t count, off_t offset);
                virtual int truncate(boost::filesystem::path v_path, int fd, off_t length);

                virtual int access(boost::filesystem::path v_path, int mode);
                virtual int unlink(boost::filesystem::path v_path);

                virtual int link(boost::filesystem::path oldpath, const boost::filesystem::path newpath);
                virtual int symlink(boost::filesystem::path oldpath, const boost::filesystem::path newpath);
                virtual int mkfifo(boost::filesystem::path v_path, mode_t mode);
                virtual int mknod(boost::filesystem::path v_path, mode_t mode, dev_t dev);

                virtual int fsync(boost::filesystem::path v_path, int fd);

                virtual int lock(boost::filesystem::path v_path, int fd, int cmd, struct ::flock *lck, const uint64_t *lock_owner);


                virtual int setxattr(boost::filesystem::path v_path, const std::string attrname, const char *attrval, size_t attrvalsize, int flags);
                virtual int getxattr(boost::filesystem::path v_path, const std::string attrname, char *buf, size_t count);
                virtual int listxattr(boost::filesystem::path v_path, char *buf, size_t count);
                virtual int removexattr(boost::filesystem::path v_path, const std::string attrname);
        };
    }
}

#endif
