#ifndef SPRINGY_FSOPS_DECORATOR_LOCAL_HPP
#define SPRINGY_FSOPS_DECORATOR_LOCAL_HPP

#include "abstract.hpp"

namespace Springy{
    namespace FsOps{
        class Local : public ::Springy::FsOps::Abstract{
            protected:
                virtual Abstract::VolumeInfo findVolume(const boost::filesystem::path file_name);
                virtual Abstract::VolumeInfo getMaxFreeSpaceVolume(const boost::filesystem::path path);
                virtual Springy::Volumes::VolumeRelativeFile getVolumesByVirtualFileName(const boost::filesystem::path file_name);

            public:
                Local(Springy::Settings *config, Springy::LibC::ILibC *libc);
                virtual ~Local();
        };
    }
}


#endif /* LOCAL_HPP */

