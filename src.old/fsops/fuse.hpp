#ifndef SPRINGY_FSOPS_FUSE_HPP
#define SPRINGY_FSOPS_FUSE_HPP

#include "abstract.hpp"

namespace Springy{
    namespace FsOps{
        class Fuse : public Abstract{
            protected:
                virtual Abstract::VolumeInfo findVolume(const boost::filesystem::path file_name);
                virtual Abstract::VolumeInfo getMaxFreeSpaceVolume(const boost::filesystem::path path);
                virtual Springy::Volumes::VolumeRelativeFile getVolumesByVirtualFileName(const boost::filesystem::path file_name);
                
                void move_file(int fd, boost::filesystem::path file, Springy::Volume::IVolume *from, fsblkcnt_t wsize);

            public:
                Fuse(Springy::Settings *config, Springy::LibC::ILibC *libc);
                virtual ~Fuse();

                virtual int create(MetaRequest meta, const boost::filesystem::path file, mode_t mode, struct ::fuse_file_info *fi);
                virtual int open(MetaRequest meta, const boost::filesystem::path file, struct ::fuse_file_info *fi);
                virtual int release(MetaRequest meta, const boost::filesystem::path path, struct ::fuse_file_info *fi);

                virtual int read(MetaRequest meta, const boost::filesystem::path file, char *buf, size_t count, off_t offset, struct ::fuse_file_info *fi);
                virtual int write(MetaRequest meta, const boost::filesystem::path file, const char *buf, size_t count, off_t offset, struct ::fuse_file_info *fi);

                virtual int lock(MetaRequest meta, const boost::filesystem::path path, int fd, int cmd, struct ::flock *lck, const void *owner, size_t owner_len);

                virtual int fsync(MetaRequest meta, const boost::filesystem::path path, int isdatasync, struct fuse_file_info *fi);
                virtual int ftruncate(MetaRequest meta, const boost::filesystem::path path, off_t size, struct fuse_file_info *fi);

                virtual int setxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname,
                                     const char *attrval, size_t attrvalsize, int flags);
                virtual int getxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname, char *buf, size_t count);
                virtual int listxattr(MetaRequest meta, const boost::filesystem::path file_name, char *buf, size_t count);
                virtual int removexattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname);
        };
    }
}

#endif /* FUSE_HPP */

