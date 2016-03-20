#ifndef SPRINGY_VOLUMES
#define SPRINGY_VOLUMES

#include "util/uri.hpp"
#include "volume/ivolume.hpp"
#include "libc/ilibc.hpp"

#include <map>
#include <set>
#include <boost/property_tree/ptree.hpp>

namespace Springy{
    class Volumes{
        protected:
            struct VolumeConfig{
                boost::filesystem::path virtualMountPoint;
                Springy::Volume::IVolume *volume;
            };

            boost::property_tree::ptree volumesTree;
            Springy::LibC::ILibC *libc;
            
            std::vector<Springy::Volumes::VolumeConfig>* getTreePropertyByPath(boost::filesystem::path p);
            void putTreePropertyByPath(boost::filesystem::path p, std::vector<Springy::Volumes::VolumeConfig> *vols);

        public:
            struct VolumeRelativeFile{
                boost::filesystem::path virtualMountPoint;
                boost::filesystem::path volumeRelativeFileName;
                std::set<Springy::Volume::IVolume*> volumes;
            };
            typedef struct VolumeRelativeFile VolumeRelativeFile;

            typedef std::map<boost::filesystem::path, std::vector<Springy::Volume::IVolume*> > VolumesMap;
            VolumesMap volumes;

            Volumes(Springy::LibC::ILibC *libc);
            ~Volumes();

            void addVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint=boost::filesystem::path("/"));
            void removeVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint=boost::filesystem::path("/"));

            Springy::Volumes::VolumeRelativeFile getVolumesByVirtualFileName(const boost::filesystem::path file_name);
            Springy::Volumes::VolumesMap getVolumes();

            boost::filesystem::path convertFuseFilenameToVolumeRelativeFilename(Springy::Volume::IVolume *volume, const boost::filesystem::path fuseFileName);
    };
}

#endif
