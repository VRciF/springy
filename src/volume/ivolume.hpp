#ifndef SPRINGY_VOLUME_IVOLUME
#define SPRINGY_VOLUME_IVOLUME

#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <boost/filesystem.hpp>

#include <unordered_map>

/*
 * upcoming volumes:
 *     o) another springy instance volume
 *     o) encrypted volume using encrypted sqlite database file
 *        https://github.com/OlivierJG/botansqlite3
 *        http://botan.randombit.net/
 *     o) webdav
 *     o) ftp
 *     o) generic database volume - maybe a filename v_file_name can be a query?
 */

namespace Springy{
    namespace Volume{
        class IVolume{
            public:
                virtual ~IVolume(){}
                virtual std::string string() = 0;
                virtual bool isLocal() = 0;

                // path based operations

                virtual int getattr(boost::filesystem::path v_file_name, struct ::stat *buf) = 0;

                virtual int statvfs(boost::filesystem::path v_path, struct ::statvfs *stat) = 0;

                virtual int chown(boost::filesystem::path v_file_name, uid_t owner, gid_t group) = 0;

                virtual int chmod(boost::filesystem::path v_file_name, mode_t mode) = 0;
                virtual int mkdir(boost::filesystem::path v_file_name, mode_t mode) = 0;
                virtual int rmdir(boost::filesystem::path v_path) = 0;

                virtual int rename(boost::filesystem::path v_old_name, boost::filesystem::path v_new_name) = 0;

                virtual int utimensat(boost::filesystem::path v_path, const struct timespec times[2]) = 0;

                virtual int readdir(boost::filesystem::path v_path, std::unordered_map<std::string, struct stat> &result) = 0;
                virtual ssize_t readlink(boost::filesystem::path v_path, char *buf, size_t bufsiz) = 0;

                virtual int access(boost::filesystem::path v_path, int mode) = 0;
                virtual int unlink(boost::filesystem::path v_path) = 0;

                virtual int link(boost::filesystem::path oldpath, const boost::filesystem::path newpath) = 0;
                virtual int symlink(boost::filesystem::path oldpath, const boost::filesystem::path newpath) = 0;
                virtual int mkfifo(boost::filesystem::path v_path, mode_t mode) = 0;
                virtual int mknod(boost::filesystem::path v_path, mode_t mode, dev_t dev) = 0;

                // descriptor based operations

                virtual int open(boost::filesystem::path v_file_name, int flags, mode_t mode=0) = 0;
                virtual int creat(boost::filesystem::path v_file_name, mode_t mode) = 0;
                virtual int close(boost::filesystem::path v_file_name, int fd) = 0;

                virtual ssize_t write(boost::filesystem::path v_file_name, int fd, const void *buf, size_t count, off_t offset) = 0;
                virtual ssize_t read(boost::filesystem::path v_file_name, int fd, void *buf, size_t count, off_t offset) = 0;
                virtual int truncate(boost::filesystem::path v_path, int fd, off_t length) = 0;

                virtual int fsync(boost::filesystem::path v_path, int fd) = 0;
                
                virtual int lock(boost::filesystem::path v_path, int fd, int cmd, struct ::flock *lck, const uint64_t *lock_owner) = 0;
        };
    }
}

#endif
