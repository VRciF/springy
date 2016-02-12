#ifndef SPRINGY_VOLUMES
#define SPRINGY_VOLUMES

#include "util/uri.hpp"
#include "volume/ivolume.hpp"
#include "libc/ilibc.hpp"

namespace Springy{
    class Volumes{
        protected:
            typedef struct {
                boost::filesystem::path virtualMountPoint;
                Springy::Util::Uri u;
                Springy::Volume::IVolume *volume;
            } VolumeConfig;
            typedef std::vector<VolumeConfig> VolumesVector;

            VolumesVector volumes_vec;
            Springy::LibC::ILibC *libc;

        public:

            Volumes(Springy::LibC::ILibC *libc);
            ~Volumes();

            void addVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint=boost::filesystem::path("/"));
            void removeVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint=boost::filesystem::path("/"));

            std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > getVolumesByVirtualFileName(boost::filesystem::path file_name);
    };
}

#endif
