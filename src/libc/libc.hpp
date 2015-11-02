#ifndef SPRINGY_LIBC_LIBC_HPP
#define SPRINGY_LIBC_LIBC_HPP

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

namespace Springy{
    namespace LibC{
        class LibC : public Springy::LibC::ILibC{
            public:
                LibC(){}
                virtual ~LibC(){}

                virtual void *malloc(size_t size){ return ::malloc(size); }
                virtual void free(void *ptr){ ::free(ptr); }
                virtual void *calloc(size_t nmemb, size_t size){ return ::calloc(nmemb, size); }
                virtual void *realloc(void *ptr, size_t size){ return ::realloc(ptr, size); }

                virtual int chown(const char *path, uid_t owner, gid_t group){ return ::chown(path, owner, group); }
                virtual int fchown(int fd, uid_t owner, gid_t group){ return ::fchown(fd, owner, group); }
                virtual int lchown(const char *path, uid_t owner, gid_t group){ return ::lchown(path, owner, group); }

                virtual int stat(const char *path, struct stat *buf){ return ::stat(path, buf); }
                virtual int fstat(int fd, struct stat *buf){ return ::fstat(fd, buf); }
                virtual int lstat(const char *path, struct stat *buf){ return ::lstat(path, buf); }

                virtual int chmod(const char *path, mode_t mode){ return ::chmod(path, mode); }
                virtual int fchmod(int fd, mode_t mode){ return ::fchmod(fd, mode); }

                virtual int mkdir(const char *pathname, mode_t mode){ return ::mkdir(pathname, mode); }
#ifndef WITHOUT_XATTR
                virtual ssize_t listxattr(const char *path, char *list, size_t size){ return ::listxattr(path, list, size); }
                virtual ssize_t llistxattr(const char *path, char *list, size_t size){ return ::llistxattr(path, list, size); }
                virtual ssize_t flistxattr(int filedes, char *list, size_t size){ return ::flistxattr(filedes, list, size); }
                virtual ssize_t getxattr (const char *path, const char *name, void *value, size_t size){ return ::getxattr(path, name, value, size); }
                virtual ssize_t lgetxattr (const char *path, const char *name, void *value, size_t size){ return ::lgetxattr(path, name, value, size); }
                virtual ssize_t fgetxattr (int filedes, const char *name, void *value, size_t size){ return ::fgetxattr(filedes, name, value, size); }
                virtual int setxattr (const char *path, const char *name, const void *value, size_t size, int flags){ return ::setxattr(path, name, value, size, flags); }
                virtual int lsetxattr (const char *path, const char *name, const void *value, size_t size, int flags){ return ::lsetxattr(path, name, value, size, flags); }
                virtual int fsetxattr (int filedes, const char *name, const void *value, size_t size, int flags){ return ::fsetxattr(filedes, name, value, size, flags); }
                virtual int removexattr (const char *path, const char *name){ return ::removexattr(path, name); }
                virtual int lremovexattr (const char *path, const char *name){ return ::lremovexattr(path, name); }
                virtual int fremovexattr (int filedes, const char *name){ return ::fremovexattr(filedes, name); }
#endif
                virtual struct dirent *readdir(DIR *dirp){ return ::readdir(dirp); }
                virtual int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result){ return ::readdir_r(dirp, entry, result); }
                virtual DIR *opendir(const char *name){ return ::opendir(name); }
                virtual DIR *fdopendir(int fd){ return ::fdopendir(fd); }
                virtual int closedir(DIR *dirp){ return ::closedir(dirp); }

                virtual int strcmp(const char *s1, const char *s2){ return ::strcmp(s1, s2); }
                virtual int strncmp(const char *s1, const char *s2, size_t n){ return ::strncmp(s1, s2, n); }
                
                virtual int open(const char *pathname, int flags){ return ::open(pathname, flags); }
                virtual int open(const char *pathname, int flags, mode_t mode){ return ::open(pathname, flags, mode); }
                virtual int creat(const char *pathname, mode_t mode){ return ::creat(pathname, mode); }
                virtual int close(int fd){ return ::close(fd); }
                virtual int fclose(FILE *fp){ return ::fclose(fp); }
                virtual FILE *fopen(const char *path, const char *mode){ return ::fopen(path, mode); }
                virtual FILE *fdopen(int fd, const char *mode){ return ::fdopen(fd, mode); }
                virtual FILE *freopen(const char *path, const char *mode, FILE *stream){ return ::freopen(path, mode, stream); }
                virtual int unlink(const char *pathname){ return ::unlink(pathname); }
                virtual int dup(int oldfd){ return ::dup(oldfd); }
                virtual int dup2(int oldfd, int newfd){ return ::dup2(oldfd, newfd); }
#ifdef _GNU_SOURCE
                virtual int dup3(int oldfd, int newfd, int flags){ return ::dup3(oldfd, newfd, flags); }
#endif

                virtual off_t lseek(int fd, off_t offset, int whence){ return ::lseek(fd, offset, whence); }
                virtual int truncate(const char *path, off_t length){ return ::truncate(path, length); }
                virtual int ftruncate(int fd, off_t length){ return ::ftruncate(fd, length); }
                virtual ssize_t pread(int fd, void *buf, size_t count, off_t offset){ return ::pread(fd, buf, count, offset); }
                virtual ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset){ return ::pwrite(fd, buf, count, offset); }
                virtual ssize_t write(int fd, const void *buf, size_t count){ return ::write(fd, buf, count); }
                virtual ssize_t read(int fd, void *buf, size_t count){ return ::read(fd, buf, count); }

                virtual int statvfs(const char *path, struct statvfs *buf){ return ::statvfs(path, buf); }
                virtual int fstatvfs(int fd, struct statvfs *buf){ return ::fstatvfs(fd, buf); }
                virtual ssize_t readlink(const char *path, char *buf, size_t bufsiz){ return ::readlink(path, buf, bufsiz); }
                virtual int access(const char *pathname, int mode){ return ::access(pathname, mode); }
                virtual int rmdir(const char *pathname){ return ::rmdir(pathname); }
                virtual int rename(const char *oldpath, const char *newpath){ return ::rename(oldpath, newpath); }
                virtual int futimes(int fd, const struct timeval tv[2]){ return ::futimes(fd, tv); }
                virtual int lutimes(const char *filename, const struct timeval tv[2]){ return ::lutimes(filename, tv); }
                virtual int link(const char *oldpath, const char *newpath){ return ::link(oldpath, newpath); }
                virtual int mkfifo(const char *pathname, mode_t mode){ return ::mkfifo(pathname, mode); }
                virtual int fsync(int fd){ return ::fsync(fd); }
                virtual int fdatasync(int fd){ return ::fdatasync(fd); }
                virtual int mknod(const char *pathname, mode_t mode, dev_t dev){ return ::mknod(pathname, mode, dev); }

                virtual uid_t getuid(void){ return ::getuid(); }
                virtual uid_t geteuid(void){ return ::geteuid(); }
                virtual gid_t getgid(void){ return ::getgid(); }
                virtual gid_t getegid(void){ return ::getegid(); }

                virtual int utime(const char *filename, const struct utimbuf *times){ return ::utime(filename, times); }
                virtual int utimes(const char *filename, const struct timeval times[2]){ return ::utimes(filename, times); }
                
                virtual int getrlimit(int resource, struct rlimit *rlim){ return ::getrlimit(resource, rlim); }
                virtual int setrlimit(int resource, const struct rlimit *rlim){ return ::setrlimit(resource, rlim); }
                virtual int prlimit(pid_t pid, __rlimit_resource resource, const struct rlimit *new_limit, struct rlimit *old_limit){ return ::prlimit(pid, resource, new_limit, old_limit); }

                virtual int umount(const char *target){ return ::umount(target); }
                virtual int umount2(const char *target, int flags){ return ::umount2(target, flags); }
                virtual int system(const char *command){ return ::system(command); }
        };
    }
}

#endif
