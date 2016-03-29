#ifndef SPRINGY_FSOPS_FUSE_HPP
#define SPRINGY_FSOPS_FUSE_HPP

#include <fuse.h>

#include "abstract.hpp"

namespace Springy{
    namespace FsOps{
        class Fuse : public Abstract{
            protected:
                struct of_idx_volumeFile{};
                struct of_idx_fd{};
                struct openFile{
                    boost::filesystem::path volumeFile;

                    Springy::Volume::IVolume *volume;

                    int fd; // file descriptor is unique
                    int flags;
                    mutable int *syncToken;

                    mutable bool valid;

                    bool operator<(const openFile& o)const{return fd < o.fd; }
                };

                typedef boost::multi_index::multi_index_container<
                  openFile,
                  boost::multi_index::indexed_by<
                    // sort by openFile::operator<
                    boost::multi_index::ordered_unique<boost::multi_index::tag<of_idx_fd>, boost::multi_index::member<openFile,int,&openFile::fd> >,

                    // sort by less<string> on name
                    boost::multi_index::ordered_non_unique<boost::multi_index::tag<of_idx_volumeFile>,boost::multi_index::member<openFile,boost::filesystem::path,&openFile::volumeFile> >
                  > 
                > openFiles_set;

                openFiles_set openFiles;

                void saveFd(boost::filesystem::path file, Springy::Volume::IVolume *volume, int fd, int flags);
                void move_file(int fd, boost::filesystem::path file, Springy::Volume::IVolume *from, fsblkcnt_t wsize);

                virtual Abstract::VolumeInfo findVolume(const boost::filesystem::path file_name);
                virtual Abstract::VolumeInfo getMaxFreeSpaceVolume(const boost::filesystem::path path);

            public:
                Fuse(Springy::Settings *config, Springy::LibC::ILibC *libc);
                virtual ~Fuse();

                virtual int create(MetaRequest meta, const boost::filesystem::path file, mode_t mode, struct ::fuse_file_info *fi);
                virtual int open(MetaRequest meta, const boost::filesystem::path file, struct ::fuse_file_info *fi);
                virtual int release(MetaRequest meta, const boost::filesystem::path path, struct ::fuse_file_info *fi);

                virtual int read(MetaRequest meta, const boost::filesystem::path file, char *buf, size_t count, off_t offset, struct ::fuse_file_info *fi);
                virtual int write(MetaRequest meta, const boost::filesystem::path file, const char *buf, size_t count, off_t offset, struct ::fuse_file_info *fi);

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

