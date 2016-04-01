#include "local.hpp"

::Springy::FsOps::Abstract *fsops;

namespace Springy {
    namespace FsOps{
        Local::Local(Springy::Settings *config, Springy::LibC::ILibC *libc) : Fuse(config, libc){}
        Local::~Local(){}


        Abstract::VolumeInfo Local::findVolume(const boost::filesystem::path file_name) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            Abstract::VolumeInfo vinfo;

            Springy::Volumes::VolumeRelativeFile vols = this->getVolumesByVirtualFileName(file_name);
            vinfo.virtualMountPoint = vols.virtualMountPoint;
            vinfo.volumeRelativeFileName = vols.volumeRelativeFileName;

            std::set<Springy::Volume::IVolume*>::iterator it;
            for (it = vols.volumes.begin(); it != vols.volumes.end(); it++) {
                Springy::Volume::IVolume *volume = *it;
                if (volume->getattr(vols.volumeRelativeFileName, &vinfo.st) != -1 && volume->isLocal()) {
                    vinfo.volume = volume;
                    volume->statvfs(vols.volumeRelativeFileName, &vinfo.stvfs);
                    //vinfo.curspace = vinfo.buf.f_bsize * vinfo.buf.f_bavail;

                    return vinfo;
                }
            }

            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "file not found";
        }

        Abstract::VolumeInfo Local::getMaxFreeSpaceVolume(const boost::filesystem::path path) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            Springy::Volumes::VolumeRelativeFile vols = this->getVolumesByVirtualFileName(path);

            Abstract::VolumeInfo vinfo;
            vinfo.virtualMountPoint = vols.virtualMountPoint;
            vinfo.volumeRelativeFileName = vols.volumeRelativeFileName;

            Springy::Volume::IVolume* maxFreeSpaceVolume = NULL;
            struct statvfs stat;

            uintmax_t space = 0;

            std::set<Springy::Volume::IVolume*>::iterator it;
            for (it = vols.volumes.begin(); it != vols.volumes.end(); it++) {
                Springy::Volume::IVolume *volume = *it;
                struct statvfs stvfs;

                volume->statvfs(vols.volumeRelativeFileName, &stvfs);
                uintmax_t curspace = stat.f_bsize * stat.f_bavail;

                if ((maxFreeSpaceVolume == NULL || curspace > space) && volume->isLocal()) {
                    vinfo.volume = volume;
                    vinfo.stvfs = stvfs;
                    volume->getattr(vols.volumeRelativeFileName, &vinfo.st);
                    maxFreeSpaceVolume = volume;

                    space = curspace;
                }
            }
            if (maxFreeSpaceVolume) {
                return vinfo;
            }

            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "no space";
        }

        Springy::Volumes::VolumeRelativeFile Local::getVolumesByVirtualFileName(const boost::filesystem::path file_name){
            Springy::Volumes::VolumeRelativeFile vrel = this->config->volumes.getVolumesByVirtualFileName(file_name);

            std::set<Springy::Volume::IVolume*>::iterator vit, tmpit;
            for(vit=vrel.volumes.begin();vit != vrel.volumes.end();){
                // remove non local volumes
                if(!(*vit)->isLocal()){
                    tmpit = vit;
                    vit++;
                    vrel.volumes.erase(tmpit);
                    continue;
                }
                vit++;
            }

            if(vrel.volumes.size() <= 0){
                throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "file not found";
            }

            return vrel;
        }
    }
}
