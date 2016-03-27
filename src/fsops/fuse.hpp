#ifndef SPRINGY_FSOPS_FUSE_HPP
#define SPRINGY_FSOPS_FUSE_HPP

#include "abstract.hpp"

namespace Springy{
    namespace FsOps{
        class Fuse : public Abstract{
            protected:
                virtual VolumeInfo findVolume(const boost::filesystem::path file_name);
                virtual VolumeInfo getMaxFreeSpaceVolume(const boost::filesystem::path path);

            public:
                Fuse(Springy::Settings *config, Springy::LibC::ILibC *libc);
                virtual ~Fuse();

                virtual int lock(MetaRequest meta, const boost::filesystem::path path, int fd, int cmd, struct ::flock *lck, const void *owner, size_t owner_len);
                virtual int setxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname,
                                     const char *attrval, size_t attrvalsize, int flags);
                virtual int getxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname, char *buf, size_t count);
                virtual int listxattr(MetaRequest meta, const boost::filesystem::path file_name, char *buf, size_t count);
                virtual int removexattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname);
                
        };
    }
}

#endif /* FUSE_HPP */

