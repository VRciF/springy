#include "file.hpp"

namespace Springy{
    namespace Volume{
        File::File(Springy::LibC::ILibC *libc, Springy::Util::Uri &u){
            this->libc = libc;
            this->u = u;
        }
        ~File(){}

        int File::lstat(boost::filesystem::path v_file_name, struct stat *buf){
            boost::filesystem::path p = boost::filesystem::path(this->u.path())/v_file_name;
            return this->libc->lstat(p.c_str(), buf);
        }
        int File::statvfs(boost::filesystem::path v_path, struct ::statvfs *stat){
            boost::filesystem::path p = boost::filesystem::path(this->u.path())/v_path;
            return this->libc->statvfs(p.c_str(), stat);
        }
    }
}
