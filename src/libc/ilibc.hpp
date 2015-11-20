#ifndef SPRINGY_LIBC_ILIBC_HPP
#define SPRINGY_LIBC_ILIBC_HPP

#include <stddef.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/resource.h>

namespace Springy{
    namespace LibC{
        class ILibC{
            public:
                virtual ~ILibC(){}

                virtual void *malloc(size_t size) = 0;
                virtual void free(void *ptr) = 0;
                virtual void *calloc(size_t nmemb, size_t size) = 0;
                virtual void *realloc(void *ptr, size_t size) = 0;

                virtual int chown(const char *path, uid_t owner, gid_t group) = 0;
                virtual int fchown(int fd, uid_t owner, gid_t group) = 0;
                virtual int lchown(const char *path, uid_t owner, gid_t group) = 0;

                virtual int stat(const char *path, struct stat *buf) = 0;
                virtual int fstat(int fd, struct stat *buf) = 0;
                virtual int lstat(const char *path, struct stat *buf) = 0;
                
                virtual int chmod(const char *path, mode_t mode) = 0;
                virtual int fchmod(int fd, mode_t mode) = 0;

                virtual int mkdir(const char *pathname, mode_t mode) = 0;

#ifndef WITHOUT_XATTR
                virtual ssize_t listxattr(const char *path, char *list, size_t size) = 0;
                virtual ssize_t llistxattr(const char *path, char *list, size_t size) = 0;
                virtual ssize_t flistxattr(int filedes, char *list, size_t size) = 0;
                virtual ssize_t getxattr (const char *path, const char *name, void *value, size_t size) = 0;
                virtual ssize_t lgetxattr (const char *path, const char *name, void *value, size_t size) = 0;
                virtual ssize_t fgetxattr (int filedes, const char *name, void *value, size_t size) = 0;
                virtual int setxattr (const char *path, const char *name, const void *value, size_t size, int flags) = 0;
                virtual int lsetxattr (const char *path, const char *name, const void *value, size_t size, int flags) = 0;
                virtual int fsetxattr (int filedes, const char *name, const void *value, size_t size, int flags) = 0;
                virtual int removexattr (const char *path, const char *name) = 0;
                virtual int lremovexattr (const char *path, const char *name) = 0;
                virtual int fremovexattr (int filedes, const char *name) = 0;
#endif
                virtual struct dirent *readdir(DIR *dirp) = 0;
                virtual int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result) = 0;
                virtual DIR *opendir(const char *name) = 0;
                virtual DIR *fdopendir(int fd) = 0;
                virtual int closedir(DIR *dirp) = 0;

                virtual int strcmp(const char *s1, const char *s2) = 0;
                virtual int strncmp(const char *s1, const char *s2, size_t n) = 0;

                virtual int open(const char *pathname, int flags) = 0;
                virtual int open(const char *pathname, int flags, mode_t mode) = 0;
                virtual int creat(const char *pathname, mode_t mode) = 0;
                virtual int close(int fd) = 0;
                virtual int fclose(FILE *fp) = 0;
                virtual FILE *fopen(const char *path, const char *mode) = 0;
                virtual FILE *fdopen(int fd, const char *mode) = 0;
                virtual FILE *freopen(const char *path, const char *mode, FILE *stream) = 0;
                virtual int unlink(const char *pathname) = 0;
                virtual int dup(int oldfd) = 0;
                virtual int dup2(int oldfd, int newfd) = 0;
#ifdef _GNU_SOURCE
                virtual int dup3(int oldfd, int newfd, int flags) = 0;
#endif
                virtual off_t lseek(int fd, off_t offset, int whence) = 0;
                virtual int truncate(const char *path, off_t length) = 0;
                virtual int ftruncate(int fd, off_t length) = 0;
                virtual ssize_t pread(int fd, void *buf, size_t count, off_t offset) = 0;
                virtual ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) = 0;
                virtual ssize_t write(int fd, const void *buf, size_t count) = 0;
                virtual ssize_t read(int fd, void *buf, size_t count) = 0;

                virtual int statvfs(const char *path, struct statvfs *buf) = 0;
                virtual int fstatvfs(int fd, struct statvfs *buf) = 0;
                virtual ssize_t readlink(const char *path, char *buf, size_t bufsiz) = 0;
                virtual int access(const char *pathname, int mode) = 0;
                virtual int rmdir(const char *pathname) = 0;
                virtual int rename(const char *oldpath, const char *newpath) = 0;
                virtual int futimes(int fd, const struct timeval tv[2]) = 0;
                virtual int lutimes(const char *filename, const struct timeval tv[2]) = 0;
                virtual int utimensat(int dirfd, const char *pathname, const struct timespec times[2], int flags) = 0;
                virtual int futimens(int fd, const struct timespec times[2]) = 0;

                virtual int link(const char *oldpath, const char *newpath) = 0;
                virtual int mkfifo(const char *pathname, mode_t mode) = 0;
                virtual int fsync(int fd) = 0;
                virtual int fdatasync(int fd) = 0;
                virtual int mknod(const char *pathname, mode_t mode, dev_t dev) = 0;
                virtual int symlink(const char *oldpath, const char *newpath) = 0;

                virtual uid_t getuid(void) = 0;
                virtual uid_t geteuid(void) = 0;
                virtual gid_t getgid(void) = 0;
                virtual gid_t getegid(void) = 0;

                virtual int utime(const char *filename, const struct utimbuf *times) = 0;
                virtual int utimes(const char *filename, const struct timeval times[2]) = 0;

                virtual int getrlimit(int resource, struct rlimit *rlim) = 0;
                virtual int setrlimit(int resource, const struct rlimit *rlim) = 0;
                virtual int prlimit(pid_t pid, __rlimit_resource resource, const struct rlimit *new_limit, struct rlimit *old_limit) = 0;

                virtual int umount(const char *target) = 0;
                virtual int umount2(const char *target, int flags) = 0;
                virtual int system(const char *command) = 0;
        };
    }
}

#endif
