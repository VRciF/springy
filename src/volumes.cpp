#include "volumes.hpp"
#include "trace.hpp"
#include "exception.hpp"

#include "volume/file.hpp"

namespace Springy{

Volumes::Volumes(Springy::LibC::ILibC *libc){ this->libc = libc; }
Volumes::~Volumes(){}

int Volumes::countEquals(const boost::filesystem::path &p1, const boost::filesystem::path &p2){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    int cnt = 0;
    boost::filesystem::path::iterator it1, it2;
    for(it1=p1.begin(), it2=p2.begin();
        it1!=p1.end() && it2!=p2.end();
        it1++,it2++){
        if(*it1 == *it2){
            if(it1->string() == "/" ||
               it2->string() == "/"){ continue; }

            cnt++;
        }
        else{
            break;
        }
    }

    return cnt;
}
int Volumes::countDirectoryElements(boost::filesystem::path p){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	int depth = 0;

	while(p.has_relative_path()){
        if(p.string() == "/"){ break; }
		depth++;
		p = p.parent_path();
	}

	return depth;
}

void Volumes::addVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint){
    std::string protocol = u.protocol();
    if(protocol != "file"){
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__, "unkown uri protocol") << u.protocol();
    }

    Synchronized syncToken(this->volumes_vec);

    std::string stru = u.string();
    VolumesVector::iterator it;
    for(it=this->volumes_vec.begin();it != this->volumes_vec.end();it++){
        if(it->virtualMountPoint == virtualMountPoint && it->volume->string() == stru){
            // already added - so silently ignore to add again
            return;
        }
    }

    Springy::Volume::IVolume *volume = NULL;
    if(protocol == "file"){
        volume = new Springy::Volume::File(this->libc, u);
    }

    VolumeConfig vcfg = {virtualMountPoint, volume};

    this->volumes_vec.push_back(vcfg);
}
void Volumes::removeVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint){
    Synchronized syncToken(this->volumes_vec);

    std::string stru = u.string();
    VolumesVector::iterator it;
    for(it=this->volumes_vec.begin();it != this->volumes_vec.end();it++){
        if(it->virtualMountPoint == virtualMountPoint && it->volume->string() == stru){
            delete it->volume;
            this->volumes_vec.erase(it);
            break;
        }
    }

    return;
}

std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > Volumes::getVolumesByVirtualFileName(const boost::filesystem::path file_name){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	Synchronized syncToken(this->volumes_vec, Synchronized::LockType::READ);

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

std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > Volumes::getVolumes(){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	Synchronized syncToken(this->volumes_vec, Synchronized::LockType::READ);

    std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > result;
    VolumesVector::iterator it;
    for(it=this->volumes_vec.begin();it != this->volumes_vec.end();it++){
        result.insert(std::make_pair(it->volume, it->virtualMountPoint));
    }
    return result;
}

}
