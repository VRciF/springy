#ifndef SPRINGY_LIBC_LIBC_HPP
#define SPRINGY_LIBC_LIBC_HPP

#include "ilibc.hpp"
#include "../trace.hpp"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <attr/xattr.h>
#include <dirent.h>
#include <string.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sys/statvfs.h>

extern "C" {
#include <ulockmgr.h>
}

namespace Springy{
    namespace LibC{
        class LibC : public Springy::LibC::ILibC{
            public:
                LibC(){}
                virtual ~LibC(){}

                virtual void *malloc(int LINE, size_t size){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::malloc(size); }
                virtual void free(int LINE, void *ptr){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); ::free(ptr); }
                virtual void *calloc(int LINE, size_t nmemb, size_t size){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::calloc(nmemb, size); }
                virtual void *realloc(int LINE, void *ptr, size_t size){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::realloc(ptr, size); }

                virtual int chown(int LINE, const char *path, uid_t owner, gid_t group){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::chown(path, owner, group); }
                virtual int fchown(int LINE, int fd, uid_t owner, gid_t group){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fchown(fd, owner, group); }
                virtual int lchown(int LINE, const char *path, uid_t owner, gid_t group){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::lchown(path, owner, group); }

                virtual int stat(int LINE, const char *path, struct ::stat *buf){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::stat(path, buf); }
                virtual int fstat(int LINE, int fd, struct ::stat *buf){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fstat(fd, buf); }
                virtual int lstat(int LINE, const char *path, struct ::stat *buf){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::lstat(path, buf); }
                
                virtual uid_t getuid(int LINE){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::getuid(); }
                virtual uid_t geteuid(int LINE){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::geteuid(); }
                virtual gid_t getgid(int LINE){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::getgid(); }
                virtual gid_t getegid(int LINE){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::getegid(); }

                virtual int chmod(int LINE, const char *path, mode_t mode){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::chmod(path, mode); }
                virtual int fchmod(int LINE, int fd, mode_t mode){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fchmod(fd, mode); }

                virtual int mkdir(int LINE, const char *pathname, mode_t mode){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::mkdir(pathname, mode); }
#ifndef WITHOUT_XATTR
                virtual ssize_t listxattr(int LINE, const char *path, char *list, size_t size){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::listxattr(path, list, size); }
                virtual ssize_t llistxattr(int LINE, const char *path, char *list, size_t size){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::llistxattr(path, list, size); }
                virtual ssize_t flistxattr(int LINE, int filedes, char *list, size_t size){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::flistxattr(filedes, list, size); }
                virtual ssize_t getxattr (int LINE, const char *path, const char *name, void *value, size_t size){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::getxattr(path, name, value, size); }
                virtual ssize_t lgetxattr (int LINE, const char *path, const char *name, void *value, size_t size){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::lgetxattr(path, name, value, size); }
                virtual ssize_t fgetxattr (int LINE, int filedes, const char *name, void *value, size_t size){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fgetxattr(filedes, name, value, size); }
                virtual int setxattr (int LINE, const char *path, const char *name, const void *value, size_t size, int flags){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::setxattr(path, name, value, size, flags); }
                virtual int lsetxattr (int LINE, const char *path, const char *name, const void *value, size_t size, int flags){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::lsetxattr(path, name, value, size, flags); }
                virtual int fsetxattr (int LINE, int filedes, const char *name, const void *value, size_t size, int flags){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fsetxattr(filedes, name, value, size, flags); }
                virtual int removexattr (int LINE, const char *path, const char *name){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::removexattr(path, name); }
                virtual int lremovexattr (int LINE, const char *path, const char *name){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::lremovexattr(path, name); }
                virtual int fremovexattr (int LINE, int filedes, const char *name){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fremovexattr(filedes, name); }
#endif
                virtual struct dirent *readdir(int LINE, DIR *dirp){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::readdir(dirp); }
                virtual int readdir_r(int LINE, DIR *dirp, struct dirent *entry, struct dirent **result){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::readdir_r(dirp, entry, result); }
                virtual DIR *opendir(int LINE, const char *name){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::opendir(name); }
                virtual DIR *fdopendir(int LINE, int fd){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fdopendir(fd); }
                virtual int closedir(int LINE, DIR *dirp){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::closedir(dirp); }

                virtual int strcmp(int LINE, const char *s1, const char *s2){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::strcmp(s1, s2); }
                virtual int strncmp(int LINE, const char *s1, const char *s2, size_t n){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::strncmp(s1, s2, n); }
                
                virtual int open(int LINE, const char *pathname, int flags){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::open(pathname, flags); }
                virtual int open(int LINE, const char *pathname, int flags, mode_t mode){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::open(pathname, flags, mode); }
                virtual int creat(int LINE, const char *pathname, mode_t mode){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::creat(pathname, mode); }
                virtual int close(int LINE, int fd){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::close(fd); }
                virtual int fclose(int LINE, FILE *fp){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fclose(fp); }
                virtual FILE *fopen(int LINE, const char *path, const char *mode){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fopen(path, mode); }
                virtual FILE *fdopen(int LINE, int fd, const char *mode){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fdopen(fd, mode); }
                virtual FILE *freopen(int LINE, const char *path, const char *mode, FILE *stream){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::freopen(path, mode, stream); }

                virtual void clearerr(int LINE, FILE *stream){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); ::clearerr(stream); }
                virtual int feof(int LINE, FILE *stream){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::feof(stream); }
                virtual int ferror(int LINE, FILE *stream){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::ferror(stream); }
                virtual int fileno(int LINE, FILE *stream){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fileno(stream); }
                
                virtual int unlink(int LINE, const char *pathname){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::unlink(pathname); }
                virtual int dup(int LINE, int oldfd){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::dup(oldfd); }
                virtual int dup2(int LINE, int oldfd, int newfd){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::dup2(oldfd, newfd); }
#ifdef _GNU_SOURCE
                virtual int dup3(int LINE, int oldfd, int newfd, int flags){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::dup3(oldfd, newfd, flags); }
#endif

                virtual off_t lseek(int LINE, int fd, off_t offset, int whence){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::lseek(fd, offset, whence); }
                virtual int truncate(int LINE, const char *path, off_t length){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::truncate(path, length); }
                virtual int ftruncate(int LINE, int fd, off_t length){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::ftruncate(fd, length); }
                virtual ssize_t pread(int LINE, int fd, void *buf, size_t count, off_t offset){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::pread(fd, buf, count, offset); }
                virtual ssize_t pwrite(int LINE, int fd, const void *buf, size_t count, off_t offset){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::pwrite(fd, buf, count, offset); }
                virtual ssize_t write(int LINE, int fd, const void *buf, size_t count){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::write(fd, buf, count); }
                virtual ssize_t read(int LINE, int fd, void *buf, size_t count){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::read(fd, buf, count); }

                virtual int statvfs(int LINE, const char *path, struct ::statvfs *buf){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::statvfs(path, buf); }
                virtual int fstatvfs(int LINE, int fd, struct ::statvfs *buf){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fstatvfs(fd, buf); }
                virtual ssize_t readlink(int LINE, const char *path, char *buf, size_t bufsiz){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::readlink(path, buf, bufsiz); }
                virtual int access(int LINE, const char *pathname, int mode){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::access(pathname, mode); }
                virtual int rmdir(int LINE, const char *pathname){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::rmdir(pathname); }
                virtual int rename(int LINE, const char *oldpath, const char *newpath){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::rename(oldpath, newpath); }
                virtual int futimes(int LINE, int fd, const struct timeval tv[2]){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::futimes(fd, tv); }
                virtual int lutimes(int LINE, const char *filename, const struct timeval tv[2]){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::lutimes(filename, tv); }
                virtual int utimensat(int LINE, int dirfd, const char *pathname, const struct timespec times[2], int flags){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::utimensat(dirfd, pathname, times, flags); }
                virtual int futimens(int LINE, int fd, const struct timespec times[2]){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::futimens(fd, times); }

                virtual int link(int LINE, const char *oldpath, const char *newpath){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::link(oldpath, newpath); }
                virtual int mkfifo(int LINE, const char *pathname, mode_t mode){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::mkfifo(pathname, mode); }
                virtual int fsync(int LINE, int fd){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fsync(fd); }
                virtual int fdatasync(int LINE, int fd){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::fdatasync(fd); }
                virtual int mknod(int LINE, const char *pathname, mode_t mode, dev_t dev){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::mknod(pathname, mode, dev); }
                virtual int symlink(int LINE, const char *oldpath, const char *newpath){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::symlink(oldpath, newpath); }

                virtual int utime(int LINE, const char *filename, const struct ::utimbuf *times){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::utime(filename, times); }
                virtual int utimes(int LINE, const char *filename, const struct timeval times[2]){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::utimes(filename, times); }
                
                virtual int getrlimit(int LINE, int resource, struct rlimit *rlim){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::getrlimit(resource, rlim); }
                virtual int setrlimit(int LINE, int resource, const struct rlimit *rlim){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::setrlimit(resource, rlim); }
                virtual int prlimit(int LINE, pid_t pid, __rlimit_resource resource, const struct rlimit *new_limit, struct rlimit *old_limit){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::prlimit(pid, resource, new_limit, old_limit); }

                virtual int umount(int LINE, const char *target){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::umount(target); }
                virtual int umount2(int LINE, const char *target, int flags){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::umount2(target, flags); }
                virtual int system(int LINE, const char *command){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::system(command); }

                virtual int abs(int LINE, int j){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::abs(j); }
                virtual long int labs(int LINE, long int j){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::labs(j); }
                virtual long long int llabs(int LINE, long long int j){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::llabs(j); }
                
                virtual char *strerror(int LINE, int errnum){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::strerror(errnum); }
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
                virtual int strerror_r(int errnum, char *buf, size_t buflen){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::strerror_r(errnum, buf, buflen); }
#else
                virtual char *strerror_r(int errnum, char *buf, size_t buflen){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::strerror_r(errnum, buf, buflen); }
#endif
                
                virtual int ulockmgr_op(int fd, int cmd, struct ::flock *lock, const void *owner, size_t owner_len){
                    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    
                    return ::ulockmgr_op(fd, cmd, lock, owner, owner_len);
                }

                virtual void* memset(int LINE, void *s, int c, size_t n){ Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__); return ::memset(s, c, n); }
        };
    }
}

#endif
