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
        
        nlohmann::json Springy::sendRequest(std::string host, int port, std::string path, nlohmann::json jparams){
            std::string params = jparams.dump();
            
            std::string response = "{errno: -1}";

            return nlohmann::json::parse(response);
        }
        
        struct stat Springy::readStatFromJson(nlohmann::json j){
            struct stat st;

            st.st_dev     = j["st_dev"];
            st.st_ino     = j["st_ino"];
            st.st_mode    = j["st_mode"];
            st.st_nlink   = j["st_nlink"];
            st.st_uid     = j["st_uid"];
            st.st_gid     = j["st_gid"];
            st.st_rdev    = j["st_rdev"];
            st.st_size    = j["st_size"];
            st.st_blksize = j["st_blksize"];
            st.st_blocks  = j["st_blocks"];
            st.st_atime   = j["st_atime"];
            st.st_mtime   = j["st_mtime"];
            st.st_ctime   = j["st_ctime"];

            return st;
        }

        int Springy::getattr(boost::filesystem::path v_file_name, struct stat *buf){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);

            nlohmann::json j;
            j["path"] = p.string();
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/getattr", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            else{
                *buf = this->readStatFromJson(j);

                return 0;
            }
        }
        int Springy::statvfs(boost::filesystem::path v_path, struct ::statvfs *stat){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/statfs", j);

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
            
            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);

            nlohmann::json j;
            j["path"] = p.string();
            j["owner"] = owner;
            j["group"] = group;
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/chown", j);
            
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
            
            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);

            nlohmann::json j;
            j["path"] = p.string();
            j["mode"] = mode;
            std::string requestData = j.dump();
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/chmod", j);

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
            
            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);

            nlohmann::json j;
            j["path"] = p.string();
            j["mode"] = mode;
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/mkdir", j);

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

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/rmdir", j);

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
            j["old"] = this->concatPath(this->u.path(), v_old_name);
            j["new"] = this->concatPath(this->u.path(), v_new_name);
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/rename", j);

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

            if(this->readonly){ errno = EROFS; return -1; }
            
            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j["times"] = nlohmann::json::array();
            j["times"][0] = {{"tv_sec", times[0].tv_sec}, {"tv_nsec", times[0].tv_nsec}};
            j["times"][1] = {{"tv_sec", times[1].tv_sec}, {"tv_nsec", times[1].tv_nsec}};
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/utimensat", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }

        int Springy::readdir(boost::filesystem::path v_path, std::unordered_map<std::string, struct stat> &result){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/readdir", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }

            // iterate the array
            nlohmann::json directories = j["diectories"];
            for (nlohmann::json::iterator it = directories.begin(); it != directories.end(); ++it) {
                nlohmann::json entry = *it;
                std::string spath = entry["path"];
                result.insert(std::make_pair(spath, this->readStatFromJson(entry)));
            }

            return 0;
        }
        
        ssize_t Springy::readlink(boost::filesystem::path v_path, char *buf, size_t bufsiz){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/readlink", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }

            std::string link = j["link"];
            if(link.size() < bufsiz){
                bufsiz = link.size();
            }
            memcpy(buf, link.c_str(), bufsiz);            

            return bufsiz;
        }

        int Springy::access(boost::filesystem::path v_path, int mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly && (mode&F_OK) != F_OK && (mode&R_OK) != R_OK){ errno = EROFS; return -1; }
            
            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j["mode"] = mode;
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/access", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            
            return 0;
        }

        int Springy::unlink(boost::filesystem::path v_path){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/unlink", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            
            return 0;
        }
        
        int Springy::link(boost::filesystem::path oldpath, const boost::filesystem::path newpath){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            nlohmann::json j;
            j["oldpath"] = this->concatPath(this->u.path(), oldpath);
            j["newpath"] = this->concatPath(this->u.path(), newpath);
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/link", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }
        int Springy::symlink(boost::filesystem::path oldpath, const boost::filesystem::path newpath){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            nlohmann::json j;
            j["oldpath"] = this->concatPath(this->u.path(), oldpath);
            j["newpath"] = this->concatPath(this->u.path(), newpath);
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/symlink", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }
        int Springy::mkfifo(boost::filesystem::path v_path, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j["mode"] = mode;
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/mkfifo", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            
            return 0;
        }
        int Springy::mknod(boost::filesystem::path v_path, mode_t mode, dev_t dev){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j["mode"] = mode;
            j["dev"] = dev;
            j = this->sendRequest(this->u.host(), this->u.port(), "/api/volume/mknod", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            
            return 0;
        }


        // descriptor based operations

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

        int Springy::truncate(boost::filesystem::path v_path, int fd, off_t length){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);
            return this->libc->truncate(__LINE__, p.c_str(), length);
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
        
        
        boost::filesystem::path Springy::concatPath(const boost::filesystem::path &p1, const boost::filesystem::path &p2){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = boost::filesystem::path(p1/p2);
            while(p.string().back() == '/'){
                p = p.parent_path();
            }

            return p;
        }

    }
}
