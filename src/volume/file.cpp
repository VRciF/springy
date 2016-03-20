#include "file.hpp"
#include "../trace.hpp"

#include <fcntl.h>
#include <sys/stat.h>
extern "C" {
#include <ulockmgr.h>
}

namespace Springy{
    namespace Volume{
        File::File(Springy::LibC::ILibC *libc, Springy::Util::Uri u) : u(u){
            this->libc = libc;
            this->readonly = (this->u.query("ro").size()>0);
        }
        File::~File(){}

        boost::filesystem::path File::concatPath(const boost::filesystem::path &p1, const boost::filesystem::path &p2){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = boost::filesystem::path(p1/p2);
            while(p.string().back() == '/'){
                p = p.parent_path();
            }

            return p;
        }

        std::string File::string(){ return this->u.string(); }
        bool File::isLocal(){ return true; }

        int File::getattr(boost::filesystem::path v_file_name, struct stat *buf){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);
            return this->libc->lstat(__LINE__, p.c_str(), buf);
        }
        int File::statvfs(boost::filesystem::path v_path, struct ::statvfs *stat){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->statvfs(__LINE__, p.c_str(), stat);
        }
        int File::chown(boost::filesystem::path v_file_name, uid_t owner, gid_t group){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);
            return this->libc->chown(__LINE__, p.c_str(), owner, group);
        }

        int File::chmod(boost::filesystem::path v_file_name, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);
            return this->libc->chmod(__LINE__, p.c_str(), mode);
        }
        int File::mkdir(boost::filesystem::path v_file_name, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);
            return this->libc->mkdir(__LINE__, p.c_str(), mode);
        }
        int File::rmdir(boost::filesystem::path v_path){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->rmdir(__LINE__, p.c_str());
        }

        int File::rename(boost::filesystem::path v_old_name, boost::filesystem::path v_new_name){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path oldp = this->concatPath(this->u.path(), v_old_name);
            boost::filesystem::path newp = this->concatPath(this->u.path(), v_new_name);
            return this->libc->rename(__LINE__, oldp.c_str(), newp.c_str());
        }

        int File::utimensat(boost::filesystem::path v_path, const struct timespec times[2]){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->utimensat(__LINE__, AT_FDCWD, p.c_str(), times, AT_SYMLINK_NOFOLLOW);
        }

        int File::readdir(boost::filesystem::path v_path, std::unordered_map<std::string, struct stat> &result){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            struct dirent *de;
            DIR * dh = this->libc->opendir(__LINE__, p.c_str());
            if (!dh){
                return errno;
            }

            while((de = this->libc->readdir(__LINE__, dh))) {
                // find dups
                if(result.find(de->d_name)!=result.end()){
                    continue;
                }

                struct ::stat st;
                this->libc->lstat(__LINE__, (p/de->d_name).c_str(), &st);
                result.insert(std::make_pair(de->d_name, st));
            }

            this->libc->closedir(__LINE__, dh);
            return 0;
        }
        
        ssize_t File::readlink(boost::filesystem::path v_path, char *buf, size_t bufsiz){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->readlink(__LINE__, p.c_str(), buf, bufsiz);
        }

        int File::open(boost::filesystem::path v_file_name, int flags, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if(this->readonly && (flags&O_RDONLY) != O_RDONLY){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);
            if(mode != 0){
                return this->libc->open(__LINE__, p.c_str(), flags);
            }
            else{
                return this->libc->open(__LINE__, p.c_str(), flags, mode);
            }
        }
        int File::creat(boost::filesystem::path v_file_name, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);
            return this->libc->creat(__LINE__, p.c_str(), mode);
        }
        int File::close(boost::filesystem::path v_file_name, int fd){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);
            return this->libc->close(__LINE__, fd);
        }
        
        ssize_t File::write(boost::filesystem::path v_file_name, int fd, const void *buf, size_t count, off_t offset){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return this->libc->pwrite(__LINE__, fd, buf, count, offset);
        }
        ssize_t File::read(boost::filesystem::path v_file_name, int fd, void *buf, size_t count, off_t offset){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return this->libc->pread(__LINE__, fd, buf, count, offset);
        }
        int File::truncate(boost::filesystem::path v_path, off_t length){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->truncate(__LINE__, p.c_str(), length);
        }

        int File::access(boost::filesystem::path v_path, int mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly && (mode&F_OK) != F_OK && (mode&R_OK) != R_OK){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->access(__LINE__, p.c_str(), mode);
        }

        int File::unlink(boost::filesystem::path v_path){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->unlink(__LINE__, p.c_str());
        }
        
        int File::link(boost::filesystem::path oldpath, const boost::filesystem::path newpath){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path oldp = this->concatPath(this->u.path(), oldpath);
            boost::filesystem::path newp = this->concatPath(this->u.path(), newpath);
            return this->libc->link(__LINE__, oldp.c_str(), newp.c_str());
        }
        int File::symlink(boost::filesystem::path oldpath, const boost::filesystem::path newpath){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path oldp = this->concatPath(this->u.path(), oldpath);
            boost::filesystem::path newp = this->concatPath(this->u.path(), newpath);
            return this->libc->symlink(__LINE__, oldp.c_str(), newp.c_str());
        }
        int File::mkfifo(boost::filesystem::path v_path, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->mkfifo(__LINE__, p.c_str(), mode);
        }
        int File::mknod(boost::filesystem::path v_path, mode_t mode, dev_t dev){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->mknod(__LINE__, p.c_str(), mode, dev);
        }

        int File::fsync(boost::filesystem::path v_path, int fd){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            return this->libc->fsync(__LINE__, fd);
        }
        int File::fdatasync(boost::filesystem::path v_path, int fd){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            return this->libc->fdatasync(__LINE__, fd);
        }
        
        int File::lock(boost::filesystem::path v_path, int fd, int cmd, struct ::flock *lck, const uint64_t *lock_owner){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            return ulockmgr_op(fd, cmd, lck, lock_owner, (size_t)sizeof(*lock_owner));
        }
    }
}
