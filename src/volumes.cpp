#include "volumes.hpp"
#include "trace.hpp"
#include "exception.hpp"

#include "volume/file.hpp"

#include <cstdint>

namespace Springy{

Volumes::Volumes(Springy::LibC::ILibC *libc){ this->libc = libc; }
Volumes::~Volumes(){}

std::vector<Springy::Volumes::VolumeConfig>* Volumes::getTreePropertyByPath(boost::filesystem::path p){
    std::vector<Springy::Volumes::VolumeConfig> *vols = NULL;

    std::string treePath = p.string();
    std::uintptr_t ui;
    if(treePath == "/"){
        ui = this->volumesTree.get_value(0);
    }
    else{
        ui = this->volumesTree.get(boost::property_tree::ptree::path_type(treePath, '/'), 0);
    }
    if(ui != 0){
        vols = reinterpret_cast<std::vector<Springy::Volumes::VolumeConfig> *>(ui);
    }
    return vols;
}
void Volumes::putTreePropertyByPath(boost::filesystem::path p, std::vector<Springy::Volumes::VolumeConfig> *vols){
    std::string treePath = p.string();

    std::uintptr_t ui = reinterpret_cast<std::uintptr_t>(vols);
    if(treePath == "/"){
        this->volumesTree.put_value(ui);
    }
    else{
        this->volumesTree.put(boost::property_tree::ptree::path_type(treePath, '/'), ui);
    }
}

void Volumes::addVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint){
    std::string protocol = u.protocol();
    if(protocol != "file" && protocol != "springy"){
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__, "unkown uri protocol") << u.protocol();
    }

    Synchronized syncToken(this->volumes);

    std::string stru = u.string();

    VolumesMap::iterator it = this->volumes.find(virtualMountPoint);
    if(it==this->volumes.end()){
        this->volumes[virtualMountPoint];
        it = this->volumes.find(virtualMountPoint);
    }
    for(size_t i=0;i<it->second.size();i++){
        if(it->second[i]->string() == stru){
            // already added - so silently ignore to add again
            return;
        }
    }

    Springy::Volume::IVolume *volume = NULL;
    if(protocol == "file"){
        volume = new Springy::Volume::File(this->libc, u);
    }
    //else if(protocol == "springy"){
    //    volume = new Springy::Volume::Springy(this->libc, u);
    //}

    it->second.push_back(volume);

    VolumeConfig vcfg = {virtualMountPoint, volume};

    std::vector<VolumeConfig> *vols = this->getTreePropertyByPath(virtualMountPoint);
    if(vols==NULL){
        vols = new std::vector<VolumeConfig>();
    }
    vols->push_back(vcfg);
    this->putTreePropertyByPath(virtualMountPoint, vols);
}
void Volumes::removeVolume(Springy::Util::Uri u, boost::filesystem::path virtualMountPoint){
    Synchronized syncToken(this->volumes);

    std::string volMount = u.string();

    VolumesMap::iterator it = this->volumes.find(virtualMountPoint);
    if(it==this->volumes.end()){
        return;
    }
    std::vector<Springy::Volume::IVolume*>::iterator vit;
    for(vit=it->second.begin();vit!=it->second.end();vit++){
        // find matching remote URI
        if((*vit)->string() == volMount){
            // remove entry from treepath
            std::vector<VolumeConfig> *vols = this->getTreePropertyByPath(virtualMountPoint);
            std::vector<VolumeConfig>::iterator vinner;
            if(vols!=NULL){
                for(vinner=vols->begin();vinner!=vols->end();vinner++){
                    if(vinner->volume->string() == volMount){
                        vols->erase(vinner);
                        break;
                    }
                }
                if(vols->size()<=0){
                    delete vols;
                    vols = NULL;
                }

                this->putTreePropertyByPath(virtualMountPoint, vols);
            }
            
            // delete IVolume*
            delete *vit;
            // and erase regarding entry
            it->second.erase(vit);

            break;
        }
    }
    if(it->second.size()<=0){
        this->volumes.erase(it);
    }
}

Springy::Volumes::VolumeRelativeFile Volumes::getVolumesByVirtualFileName(const boost::filesystem::path file_name){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	Synchronized syncToken(this->volumes, Synchronized::LockType::READ);

    Springy::Volumes::VolumeRelativeFile result;

    std::vector<VolumeConfig> *vols = this->getTreePropertyByPath(boost::filesystem::path());
    if(vols!=NULL){
        for(size_t i=0;i<vols->size();i++){
            if(result.virtualMountPoint.empty()){
                result.virtualMountPoint = (*vols)[i].virtualMountPoint;
            }
            result.volumeRelativeFileName = file_name;
            result.volumes.insert((*vols)[i].volume);
        }
    }

    boost::filesystem::path virtualMountPoint;
    boost::filesystem::path::const_iterator pit;
    for(pit=file_name.begin();pit!=file_name.end();pit++){
        boost::filesystem::path tmp = virtualMountPoint;

        tmp /= *pit;

        std::vector<VolumeConfig> *tvols = NULL;
        tvols = this->getTreePropertyByPath(tmp);
        if(tvols != NULL && tvols->size() > 0){
            result = Springy::Volumes::VolumeRelativeFile();
            for(size_t i=0;i<tvols->size();i++){
                if(result.virtualMountPoint.empty()){
                    result.virtualMountPoint = (*tvols)[i].virtualMountPoint;
                }
                boost::filesystem::path::const_iterator pit2;
                for(pit2=pit;pit2!=file_name.end();pit2++){
                    result.volumeRelativeFileName /= *pit2;
                }
                result.volumes.insert((*tvols)[i].volume);
            }
        }
    }
    
    if(result.volumes.size() <= 0){
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "no matching volumes found";
    }

    return result;
}

Springy::Volumes::VolumesMap Volumes::getVolumes(){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	Synchronized syncToken(this->volumes, Synchronized::LockType::READ);
    
    return this->volumes;
}

boost::filesystem::path Springy::Volumes::convertFuseFilenameToVolumeRelativeFilename(Springy::Volume::IVolume *volume, const boost::filesystem::path fuseFileName){
    Springy::Volumes::VolumeRelativeFile rel = Volumes::getVolumesByVirtualFileName(fuseFileName);
    std::set<Springy::Volume::IVolume*>::iterator it;
    for(it=rel.volumes.begin();it!=rel.volumes.end();it++){
        if(*it == volume){
            return rel.volumeRelativeFileName;
        }
    }
    throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "no matching volume found";
}

}
