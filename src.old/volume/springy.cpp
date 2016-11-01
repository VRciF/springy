#include "springy.hpp"
#include "../trace.hpp"
#include "../util/json.hpp"
#include "../util/string.hpp"
#include "../exception.hpp"

#include <fcntl.h>
#include <sys/stat.h>

namespace Springy{
    namespace Volume{
        Springy::Springy(::Springy::LibC::ILibC *libc, ::Springy::Util::Uri u) : u(u){
            this->libc = libc;
            this->readonly = (this->u.query("ro").size()>0);
            this->maxBodySize = 16*1024*1024;  // 16 MB
        }
        Springy::~Springy(){}

        std::string Springy::string(){ return this->u.string(); }
        bool Springy::isLocal(){ return false; }
        
        boost::filesystem::path Springy::concatPath(const boost::filesystem::path &p1, const boost::filesystem::path &p2){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = boost::filesystem::path(p1/p2);
            while(p.string().back() == '/'){
                p = p.parent_path();
            }

            return p;
        }
        
        boost::asio::ip::tcp::socket Springy::createConnection(std::string host, int port){
            std::stringstream sport;
            sport << port;

            // Get a list of endpoints corresponding to the server name.
            boost::asio::ip::tcp::resolver resolver(this->io_service);
            boost::asio::ip::tcp::resolver::query query(host.c_str(), sport.str().c_str());
            boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
            boost::asio::ip::tcp::resolver::iterator end;

            // Try each endpoint until we successfully establish a connection.
            boost::asio::ip::tcp::socket socket(this->io_service);
            boost::system::error_code error = boost::asio::error::host_not_found;
            while (error && endpoint_iterator != end)
            {
                socket.close();
                socket.connect(*endpoint_iterator++, error);
            }
            if (error)
                throw boost::system::system_error(error);

            return socket;
        }

        nlohmann::json Springy::sendRequest(std::string path, nlohmann::json jparams, boost::asio::ip::tcp::socket *socket){
            std::string params = jparams.dump();

            std::string host = this->u.host();
            int port = this->u.port();

            bool tmpSocketInUse = false;
            boost::asio::ip::tcp::socket tmpSocket(this->io_service);

            try
            {
                if(socket == NULL){
                    tmpSocket = this->createConnection(host, port);
                    socket = &tmpSocket;
                    tmpSocketInUse = true;
                }

                // Form the request. We specify the "Connection: close" header so that the
                // server will close the socket after transmitting the response. This will
                // allow us to treat all data up until the EOF as the content.
                boost::asio::streambuf request;
                std::ostream request_stream(&request);
                request_stream << "POST " << path << " HTTP/1.0\r\n";
                request_stream << "Host: " << host << "\r\n";
                request_stream << "Accept: */*\r\n";
                request_stream << "Content-Length: " << params.size() << "\r\n";
                request_stream << "Connection: keep-alive\r\n\r\n";
                request_stream << params;

                // Send the request.
                boost::asio::write(*socket, request);

                // Read the response status line.
                boost::asio::streambuf response;
                boost::asio::read_until(*socket, response, "\r\n");

                // Check that response is OK.
                std::istream response_stream(&response);
                std::string http_version;
                response_stream >> http_version;
                unsigned int status_code;
                response_stream >> status_code;
                std::string status_message;
                std::getline(response_stream, status_message);
                if (!response_stream || http_version.substr(0, 5) != "HTTP/")
                {
                    throw ::Springy::Exception(__FILE__, __LINE__) << "Invalid response";
                }
                if (status_code != 200)
                {
                    throw ::Springy::Exception(__FILE__, __LINE__) << "Invalid Status Code retunred: " << status_code;
                }

                size_t contentLength = 0;
                std::map<std::string, std::string> headers;

                while(true){
                    std::size_t n = boost::asio::read_until(*socket, response, "\r\n");
                    boost::asio::streambuf::const_buffers_type bufs = response.data();
                    std::string line(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + n);

                    if(line.size()<=0 || line=="\r\n"){
                        break;
                    }

                    std::size_t pos = line.find_first_of(":");
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos+1);
                    ::Springy::Util::String::trim(value);
                    headers.insert(std::make_pair(key, value));
                }
                if(headers.find("Content-Length")!=headers.end()){
                    contentLength = std::stoi(headers["Content-Length"]);
                }

                if(contentLength > this->maxBodySize){
                    throw boost::system::system_error(boost::asio::error::no_buffer_space);
                }

                std::string jsonResponse;
                if(contentLength > 0){
                    boost::system::error_code error;
                    std::size_t n = boost::asio::read(*socket, response, boost::asio::transfer_at_least(contentLength), error);
                    if (error)
                        throw boost::system::system_error(error);

                    boost::asio::streambuf::const_buffers_type bufs = response.data();
                    jsonResponse = std::string(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + n);
                }
                
                if(tmpSocketInUse){
                    tmpSocket.close();
                }

                return nlohmann::json::parse(jsonResponse);
            }
            catch (std::exception& e)
            {
                nlohmann::json j;
                j["errno"] = -EIO;
                j["message"] = e.what();

                if(tmpSocketInUse){
                    tmpSocket.close();
                }
                
                return j;
            }
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
            j = this->sendRequest("/api/volume/getattr", j);

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
            j = this->sendRequest("/api/volume/statfs", j);

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
            j = this->sendRequest("/api/volume/chown", j);
            
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
            j = this->sendRequest("/api/volume/chmod", j);

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
            j = this->sendRequest("/api/volume/mkdir", j);

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
            j = this->sendRequest("/api/volume/rmdir", j);

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
            j = this->sendRequest("/api/volume/rename", j);

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

            nlohmann::json j, t1, t2;
            j["path"] = p.string();
            j["times"] = nlohmann::json::array();

            t1["tv_sec"]  = times[0].tv_sec;
            t1["tv_nsec"] = times[0].tv_nsec;
            t2["tv_sec"]  = times[1].tv_sec;
            t2["tv_nsec"] = times[2].tv_nsec;

            j["times"][0] = t1;
            j["times"][1] = t2;
            j = this->sendRequest("/api/volume/utimensat", j);

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
            j = this->sendRequest("/api/volume/readdir", j);

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
            j = this->sendRequest("/api/volume/readlink", j);

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
            j = this->sendRequest("/api/volume/access", j);

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
            j = this->sendRequest("/api/volume/unlink", j);

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
            j["old"] = this->concatPath(this->u.path(), oldpath);
            j["new"] = this->concatPath(this->u.path(), newpath);
            j = this->sendRequest("/api/volume/link", j);

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
            j["old"] = this->concatPath(this->u.path(), oldpath);
            j["new"] = this->concatPath(this->u.path(), newpath);
            j = this->sendRequest("/api/volume/symlink", j);

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
            j = this->sendRequest("/api/volume/mkfifo", j);

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
            j = this->sendRequest("/api/volume/mknod", j);

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

            nlohmann::json j;
            j["path"] = p.string();
            j["flags"] = flags;
            j["mode"] = mode;
            j = this->sendRequest("/api/volume/open", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            int fd = j["fd"];
            return fd;
        }
        int Springy::creat(boost::filesystem::path v_file_name, mode_t mode){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);

            nlohmann::json j;
            j["path"] = p.string();
            j["mode"] = mode;
            j = this->sendRequest("/api/volume/creat", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            int fd = j["fd"];
            return fd;
        }
        int Springy::close(boost::filesystem::path v_file_name, int fd){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);

            nlohmann::json j;
            j["path"] = p.string();
            j["fd"] = fd;
            j = this->sendRequest("/api/volume/close", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }

        ssize_t Springy::write(boost::filesystem::path v_file_name, int fd, const void *buf, size_t count, off_t offset){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);

            nlohmann::json j;
            j["path"] = p.string();
            j["fd"] = fd;
            j["offset"] = offset;
            j["buf"] = ::Springy::Util::String::encode64(std::string((const char*)buf, count));
            j = this->sendRequest("/api/volume/write", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return j["size"];
        }
        ssize_t Springy::read(boost::filesystem::path v_file_name, int fd, void *buf, size_t count, off_t offset){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_file_name);

            nlohmann::json j;
            j["path"] = p.string();
            j["fd"] = fd;
            j["offset"] = offset;
            j["count"] = count;
            j = this->sendRequest("/api/volume/read", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }

            std::string buffer = ::Springy::Util::String::decode64(j["buf"]);
            if(buffer.size() > 0){
                memcpy(buf, buffer.data(), buffer.size());
            }
            return buffer.size();
        }

        int Springy::truncate(boost::filesystem::path v_path, int fd, off_t length){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if(this->readonly){ errno = EROFS; return -1; }

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j["fd"] = fd;
            j["size"] = length;
            j = this->sendRequest("/api/volume/truncate", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }

        int Springy::fsync(boost::filesystem::path v_path, int fd){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            if(this->readonly){ errno = EROFS; return -1; }
            
            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;
            j["path"] = p.string();
            j["fd"] = fd;
            j = this->sendRequest("/api/volume/fsync", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }

        int Springy::lock(boost::filesystem::path v_path, int fd, int cmd, struct ::flock *lck, const uint64_t *lock_owner){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j, flck;
            flck["l_type"]   = lck->l_type;
            flck["l_whence"] = lck->l_whence;
            flck["l_start"]  = lck->l_start;
            flck["l_len"]    = lck->l_len;
            flck["l_pid"]    = lck->l_pid;

            j["path"] = p.string();
            j["fd"] = fd;
            j["cmd"] = cmd;
            j["lock_owner"] = *lock_owner;
            j["lock"] = flck;
            j = this->sendRequest("/api/volume/locks", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }
        
        int Springy::setxattr(boost::filesystem::path v_path, const std::string attrname, const char *attrval, size_t attrvalsize, int flags){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;

            j["path"] = p.string();
            j["xattr"] = attrname;
            j["value"] = ::Springy::Util::String::encode64(std::string(attrval, attrvalsize));
            j["flags"] = flags;
            j = this->sendRequest("/api/volume/setxattr", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }
        int Springy::getxattr(boost::filesystem::path v_path, const std::string attrname, char *buf, size_t count){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;

            j["path"] = p.string();
            j["xattr"] = attrname;
            j = this->sendRequest("/api/volume/getxattr", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }

            std::string value = ::Springy::Util::String::decode64(j["xattr"]);
            if(buf == NULL || count == 0){
                errno = ERANGE;
                return value.size();
            }
            if(value.size()>count){
                errno = ERANGE;
                return -1;
            }
            memcpy(buf, value.data(), value.size());

            return 0;
        }
        int Springy::listxattr(boost::filesystem::path v_path, char *buf, size_t count){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;

            j["path"] = p.string();
            j = this->sendRequest("/api/volume/listxattr", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            
            size_t offset = 0;
            for (nlohmann::json::iterator it = j["xattrs"].begin(); it != j["xattrs"].end() && count > 0; ++it) {
                std::string value = *it;
                value = ::Springy::Util::String::encode64(value);
                if((value.size()+1) > count){ continue; }
                count -= value.size();
                memcpy(buf+offset, value.data(), value.size());
                offset += value.size();
                buf[offset] = '\0';
                offset++;
            }

            return 0;
        }
        int Springy::removexattr(boost::filesystem::path v_path, const std::string attrname){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path p = this->concatPath(this->u.path(), v_path);

            nlohmann::json j;

            j["path"] = p.string();
            j["xattr"] = attrname;
            j = this->sendRequest("/api/volume/removexattr", j);

            int err = j["errno"];
            err = err < 0 ? -err : err;

            if(err != 0){
                errno = err;
                return -1;
            }
            return 0;
        }
    }
}
