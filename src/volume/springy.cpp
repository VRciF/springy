#include "springy.hpp"
#include "../trace.hpp"
#include "../util/json.hpp"

#include <fcntl.h>
#include <sys/stat.h>

namespace Springy{
    namespace Volume{
        Springy::Springy(::Springy::LibC::ILibC *libc, ::Springy::Util::Uri u) : u(u){
            this->libc = libc;
            this->readonly = (this->u.query("ro").size()>0);
        }
        Springy::~Springy(){}

        std::string Springy::string(){ return this->u.string(); }
        bool Springy::isLocal(){ return false; }
        
        std::string Springy::sendRequest(std::string host, int port, std::string path, std::string &params){
            std::string response;
            return response;
        }

        int Springy::getattr(boost::filesystem::path v_file_name, struct stat *buf){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            nlohmann::json j;
            j["path"] = v_file_name.string();
            std::string requestData = j.dump();
            std::string response = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/getattr", requestData);
            j = nlohmann::json::parse(response);
            
            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            else{
                buf->st_dev     = j["st_dev"];
                buf->st_ino     = j["st_ino"];
                buf->st_mode    = j["st_mode"];
                buf->st_nlink   = j["st_nlink"];
                buf->st_uid     = j["st_uid"];
                buf->st_gid     = j["st_gid"];
                buf->st_rdev    = j["st_rdev"];
                buf->st_size    = j["st_size"];
                buf->st_blksize = j["st_blksize"];
                buf->st_blocks  = j["st_blocks"];
                buf->st_atime   = j["st_atime"];
                buf->st_mtime   = j["st_mtime"];
                buf->st_ctime   = j["st_ctime"];

                return 0;
            }
        }
        int Springy::statvfs(boost::filesystem::path v_path, struct ::statvfs *stat){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            nlohmann::json j;
            j["path"] = v_path.string();
            std::string requestData = j.dump();
            std::string response = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/statfs", requestData);
            j = nlohmann::json::parse(response);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            else{
                stat->f_bsize   = j["f_bsize"];
                stat->f_frsize  = j["f_frsize"];
                stat->f_blocks  = j["f_blocks"];
                stat->f_bfree   = j["f_bfree"];
                stat->f_bavail  = j["f_bavail"];
                stat->f_files   = j["f_files"];
                stat->f_ffree   = j["f_ffree"];
                stat->f_favail  = j["f_favail"];
                stat->f_fsid    = j["f_fsid"];
                stat->f_flag    = j["f_flag"];
                stat->f_namemax = j["f_namemax"];

                return 0;
            }
        }
        int Springy::chown(boost::filesystem::path v_file_name, uid_t owner, gid_t group){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if(this->readonly){ errno = EROFS; return -1; }

            nlohmann::json j;
            j["path"] = v_file_name.string();
            j["owner"] = owner;
            j["group"] = group;
            std::string requestData = j.dump();
            std::string response = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/chown", requestData);
            j = nlohmann::json::parse(response);
            
            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }

        int Springy::chmod(boost::filesystem::path v_file_name, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if(this->readonly){ errno = EROFS; return -1; }

            nlohmann::json j;
            j["path"] = v_file_name.string();
            j["mode"] = mode;
            std::string requestData = j.dump();
            std::string response = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/chmod", requestData);
            j = nlohmann::json::parse(response);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }
        int Springy::mkdir(boost::filesystem::path v_file_name, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            nlohmann::json j;
            j["path"] = v_file_name.string();
            j["mode"] = mode;
            std::string requestData = j.dump();
            std::string response = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/mkdir", requestData);
            j = nlohmann::json::parse(response);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }
        int Springy::rmdir(boost::filesystem::path v_path){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            nlohmann::json j;
            j["path"] = v_path.string();
            std::string requestData = j.dump();
            std::string response = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/rmdir", requestData);
            j = nlohmann::json::parse(response);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }

        int Springy::rename(boost::filesystem::path v_old_name, boost::filesystem::path v_new_name){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            nlohmann::json j;
            j["old"] = v_old_name.string();
            j["new"] = v_new_name.string();
            std::string requestData = j.dump();
            std::string response = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/rename", requestData);
            j = nlohmann::json::parse(response);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }

        int Springy::utimensat(boost::filesystem::path v_path, const struct timespec times[2]){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->utimensat(__LINE__, AT_FDCWD, p.c_str(), times, AT_SYMLINK_NOFOLLOW);
        }

        int Springy::readdir(boost::filesystem::path v_path, std::unordered_map<std::string, struct stat> &result){
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
        
        ssize_t Springy::readlink(boost::filesystem::path v_path, char *buf, size_t bufsiz){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->readlink(__LINE__, p.c_str(), buf, bufsiz);
        }

        int Springy::open(boost::filesystem::path v_file_name, int flags, mode_t mode){
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
        int Springy::creat(boost::filesystem::path v_file_name, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);
            return this->libc->creat(__LINE__, p.c_str(), mode);
        }
        int Springy::close(boost::filesystem::path v_file_name, int fd){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);
            return this->libc->close(__LINE__, fd);
        }
        
        ssize_t Springy::pread(boost::filesystem::path v_file_name, int fd, void *buf, size_t count, off_t offset){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return this->libc->pread(__LINE__, fd, buf, count, offset);
        }

        int Springy::truncate(boost::filesystem::path v_path, off_t length){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->truncate(__LINE__, p.c_str(), length);
        }
        
        int Springy::access(boost::filesystem::path v_path, int mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly && (mode&F_OK) != F_OK && (mode&R_OK) != R_OK){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->access(__LINE__, p.c_str(), mode);
        }

        int Springy::unlink(boost::filesystem::path v_path){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->unlink(__LINE__, p.c_str());
        }
        
        int Springy::link(boost::filesystem::path oldpath, const boost::filesystem::path newpath){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path oldp = this->concatPath(this->u.path(), oldpath);
            boost::filesystem::path newp = this->concatPath(this->u.path(), newpath);
            return this->libc->link(__LINE__, oldp.c_str(), newp.c_str());
        }
        int Springy::symlink(boost::filesystem::path oldpath, const boost::filesystem::path newpath){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path oldp = this->concatPath(this->u.path(), oldpath);
            boost::filesystem::path newp = this->concatPath(this->u.path(), newpath);
            return this->libc->symlink(__LINE__, oldp.c_str(), newp.c_str());
        }
        int Springy::mkfifo(boost::filesystem::path v_path, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->mkfifo(__LINE__, p.c_str(), mode);
        }
        int Springy::mknod(boost::filesystem::path v_path, mode_t mode, dev_t dev){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->mknod(__LINE__, p.c_str(), mode, dev);
        }

        int Springy::fsync(boost::filesystem::path v_path, int fd){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            return this->libc->fsync(__LINE__, fd);
        }
        int Springy::fdatasync(boost::filesystem::path v_path, int fd){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            return this->libc->fdatasync(__LINE__, fd);
        }
        
        int Springy::lock(boost::filesystem::path v_path, int fd, int cmd, struct ::flock *lck, const uint64_t *lock_owner){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            //return ulockmgr_op(fd, cmd, lck, lock_owner, (size_t)sizeof(*lock_owner));
            return 0;
        }
    }
}
