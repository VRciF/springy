#include "volumes.hpp"

namespace Springy{

Volumes::Volumes(Springy::LibC::ILibC *libc){ this->libc = libc; }
Volumes::~Volumes(){}

void Volumes::addVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint=boost::filesystem::path("/")){
    if(u.protocol() != "file"){
        throw Springy::Exception("unkown uri protocol");
    }

    Synchronized syncToken(this->volumes);
    
    std::string stru = u.string();
    VolumesVector::iterator it;
    for(it=this->volumes_vec.begin();it != this->volumes_vec.end();it++){
        if(it->virtualMountPoint == virtualMountPoint && it->u.string() == stru){
            // already added - so silently ignore to add again
            return;
        }
    }

    VolumeConfig vcfg;
    vcfg.u = u;
    vcfg.virtualMountPoint = virtualMountPoint;

    if(u.protocol() == "file"){
        Springy::Volume::File *f = 
        vcfg.volume = new Springy::Volume::File(this->libc, u);
    }

    this->volumes_vec.push_back(vcfg);
}
void Volumes::removeVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint=boost::filesystem::path("/")){
    Synchronized syncToken(this->volumes);

    std::string stru = u.string();
    VolumesVector::iterator it;
    for(it=this->volumes_vec.begin();it != this->volumes_vec.end();it++){
        if(it->virtualMountPoint == virtualMountPoint && it->u.string() == stru){
            delete it->volume;
            this->volumes_vec.erase(it);
            break;
        }
    }

    return;
}

std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > Volumes::getVolumesByVirtualFileName(boost::filesystem::path file_name){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	Synchronized syncToken(this->volumes, Synchronized::LockType::READ);

    std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > result;

    int virtualEqualCnt = 0;
    VolumesVector::iterator it;
    for(it=this->volumes_vec.begin();it != this->volumes_vec.end();it++){
        int dirParts = this->countDirectoryElements(it->virtualMountPoint);
        if(dirParts<=0){ continue; } // ignore '/' root mapping

        int equals = this->countEquals(it->virtualMountPoint, file_name);

        if(equals == dirParts){
            if(virtualEqualCnt>equals){
                continue;
            }

            if(virtualEqualCnt<equals){
                result.clear();
            }

            result.insert(std::make_pair(it->volume, it->virtualMountPoint));
            virtualEqualCnt = equals;
        }
    }

    if(result.size()<=0){
        for(it=this->volumes_vec.begin();it != this->volumes_vec.end();it++){
            int dirParts = this->countDirectoryElements(it->virtualMountPoint);
            if(dirParts>0){ continue; } // ignore non '/' root mapping

            result.insert(std::make_pair(it->volume, it->virtualMountPoint));
        }
    }

    return result;
}

}
