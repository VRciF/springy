#ifndef SPRINGY_VOLUMES
#define SPRINGY_VOLUMES

#include "util/uri.hpp"
#include "volume/ivolume.hpp"
#include "libc/ilibc.hpp"

#include <set>

namespace Springy{
    class Volumes{
        protected:
            struct VolumeConfig{
                boost::filesystem::path virtualMountPoint;
                Springy::Volume::IVolume *volume;
            };
            typedef std::vector<VolumeConfig> VolumesVector;

            VolumesVector volumes_vec;
            Springy::LibC::ILibC *libc;

            int countEquals(const boost::filesystem::path &p1, const boost::filesystem::path &p2);
            int countDirectoryElements(boost::filesystem::path p);

        public:

            Volumes(Springy::LibC::ILibC *libc);
            ~Volumes();

            void addVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint=boost::filesystem::path("/"));
            void removeVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint=boost::filesystem::path("/"));

            std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > getVolumesByVirtualFileName(const boost::filesystem::path file_name);
            std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > getVolumes();
    };
}

#endif
