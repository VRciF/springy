#ifndef SPRINGY_LIBC_ILIBC_HPP
#define SPRINGY_LIBC_ILIBC_HPP

#include <stddef.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <utime.h>

/**
 * for testing purpose springy uses this wrapper class as an interface to libc
 */

namespace Springy{
    namespace LibC{
        class ILibC{
            public:
                virtual ~ILibC(){}

                virtual void *malloc(int LINE, size_t size) = 0;
                virtual void free(int LINE, void *ptr) = 0;
                virtual void *calloc(int LINE, size_t nmemb, size_t size) = 0;
                virtual void *realloc(int LINE, void *ptr, size_t size) = 0;

                virtual int chown(int LINE, const char *path, uid_t owner, gid_t group) = 0;
                virtual int fchown(int LINE, int fd, uid_t owner, gid_t group) = 0;
                virtual int lchown(int LINE, const char *path, uid_t owner, gid_t group) = 0;

                virtual int stat(int LINE, const char *path, struct ::stat *buf) = 0;
                virtual int fstat(int LINE, int fd, struct ::stat *buf) = 0;
                virtual int lstat(int LINE, const char *path, struct ::stat *buf) = 0;

                virtual uid_t getuid(int LINE) = 0;
                virtual uid_t geteuid(int LINE) = 0;
                virtual gid_t getgid(int LINE) = 0;
                virtual gid_t getegid(int LINE) = 0;

                virtual int chmod(int LINE, const char *path, mode_t mode) = 0;
                virtual int fchmod(int LINE, int fd, mode_t mode) = 0;

                virtual int mkdir(int LINE, const char *pathname, mode_t mode) = 0;

#ifndef WITHOUT_XATTR
                virtual ssize_t listxattr(int LINE, const char *path, char *list, size_t size) = 0;
                virtual ssize_t llistxattr(int LINE, const char *path, char *list, size_t size) = 0;
                virtual ssize_t flistxattr(int LINE, int filedes, char *list, size_t size) = 0;
                virtual ssize_t getxattr(int LINE, const char *path, const char *name, void *value, size_t size) = 0;
                virtual ssize_t lgetxattr(int LINE, const char *path, const char *name, void *value, size_t size) = 0;
                virtual ssize_t fgetxattr(int LINE, int filedes, const char *name, void *value, size_t size) = 0;
                virtual int setxattr(int LINE, const char *path, const char *name, const void *value, size_t size, int flags) = 0;
                virtual int lsetxattr(int LINE, const char *path, const char *name, const void *value, size_t size, int flags) = 0;
                virtual int fsetxattr(int LINE, int filedes, const char *name, const void *value, size_t size, int flags) = 0;
                virtual int removexattr(int LINE, const char *path, const char *name) = 0;
                virtual int lremovexattr(int LINE, const char *path, const char *name) = 0;
                virtual int fremovexattr(int LINE, int filedes, const char *name) = 0;
#endif
                virtual struct dirent *readdir(int LINE, DIR *dirp) = 0;
                virtual int readdir_r(int LINE, DIR *dirp, struct dirent *entry, struct dirent **result) = 0;
                virtual DIR *opendir(int LINE, const char *name) = 0;
                virtual DIR *fdopendir(int LINE, int fd) = 0;
                virtual int closedir(int LINE, DIR *dirp) = 0;

                virtual int strcmp(int LINE, const char *s1, const char *s2) = 0;
                virtual int strncmp(int LINE, const char *s1, const char *s2, size_t n) = 0;

                virtual int open(int LINE, const char *pathname, int flags) = 0;
                virtual int open(int LINE, const char *pathname, int flags, mode_t mode) = 0;
                virtual int creat(int LINE, const char *pathname, mode_t mode) = 0;
                virtual int close(int LINE, int fd) = 0;
                virtual int fclose(int LINE, FILE *fp) = 0;
                virtual FILE *fopen(int LINE, const char *path, const char *mode) = 0;
                virtual FILE *fdopen(int LINE, int fd, const char *mode) = 0;
                virtual FILE *freopen(int LINE, const char *path, const char *mode, FILE *stream) = 0;
                
                virtual void clearerr(int LINE, FILE *stream) = 0;
                virtual int feof(int LINE, FILE *stream) = 0;
                virtual int ferror(int LINE, FILE *stream) = 0;
                virtual int fileno(int LINE, FILE *stream) = 0;

                virtual int unlink(int LINE, const char *pathname) = 0;
                virtual int dup(int LINE, int oldfd) = 0;
                virtual int dup2(int LINE, int oldfd, int newfd) = 0;
#ifdef _GNU_SOURCE
                virtual int dup3(int LINE, int oldfd, int newfd, int flags) = 0;
#endif
                virtual off_t lseek(int LINE, int fd, off_t offset, int whence) = 0;
                virtual int truncate(int LINE, const char *path, off_t length) = 0;
                virtual int ftruncate(int LINE, int fd, off_t length) = 0;
                virtual ssize_t pread(int LINE, int fd, void *buf, size_t count, off_t offset) = 0;
                virtual ssize_t pwrite(int LINE, int fd, const void *buf, size_t count, off_t offset) = 0;
                virtual ssize_t write(int LINE, int fd, const void *buf, size_t count) = 0;
                virtual ssize_t read(int LINE, int fd, void *buf, size_t count) = 0;

                virtual int statvfs(int LINE, const char *path, struct ::statvfs *buf) = 0;
                virtual int fstatvfs(int LINE, int fd, struct ::statvfs *buf) = 0;
                virtual ssize_t readlink(int LINE, const char *path, char *buf, size_t bufsiz) = 0;
                virtual int access(int LINE, const char *pathname, int mode) = 0;
                virtual int rmdir(int LINE, const char *pathname) = 0;
                virtual int rename(int LINE, const char *oldpath, const char *newpath) = 0;
                virtual int futimes(int LINE, int fd, const struct timeval tv[2]) = 0;
                virtual int lutimes(int LINE, const char *filename, const struct timeval tv[2]) = 0;
                virtual int utimensat(int LINE, int dirfd, const char *pathname, const struct timespec times[2], int flags) = 0;
                virtual int futimens(int LINE, int fd, const struct timespec times[2]) = 0;

                virtual int link(int LINE, const char *oldpath, const char *newpath) = 0;
                virtual int mkfifo(int LINE, const char *pathname, mode_t mode) = 0;
                virtual int fsync(int LINE, int fd) = 0;
                virtual int fdatasync(int LINE, int fd) = 0;
                virtual int mknod(int LINE, const char *pathname, mode_t mode, dev_t dev) = 0;
                virtual int symlink(int LINE, const char *oldpath, const char *newpath) = 0;

                virtual int utime(int LINE, const char *filename, const struct ::utimbuf *times) = 0;
                virtual int utimes(int LINE, const char *filename, const struct timeval times[2]) = 0;

                virtual int getrlimit(int LINE, int resource, struct rlimit *rlim) = 0;
                virtual int setrlimit(int LINE, int resource, const struct rlimit *rlim) = 0;
                virtual int prlimit(int LINE, pid_t pid, __rlimit_resource resource, const struct rlimit *new_limit, struct rlimit *old_limit) = 0;

                virtual int umount(int LINE, const char *target) = 0;
                virtual int umount2(int LINE, const char *target, int flags) = 0;
                virtual int system(int LINE, const char *command) = 0;
                
                virtual int abs(int LINE, int j) = 0;
                virtual long int labs(int LINE, long int j) = 0;
                virtual long long int llabs(int LINE, long long int j) = 0;
                
                virtual char *strerror(int LINE, int errnum) = 0;
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
                virtual int strerror_r(int errnum, char *buf, size_t buflen) = 0;
#else
                virtual char *strerror_r(int errnum, char *buf, size_t buflen) = 0;
#endif
                                
                virtual int ulockmgr_op(int fd, int cmd, struct ::flock *lock, const void *owner, size_t owner_len) = 0;
        };
    }
}

#endif
