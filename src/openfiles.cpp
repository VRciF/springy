#include "openfiles.hpp"
#include "util/synchronized.hpp"
#include "exception.hpp"

namespace Springy{
    OpenFiles::OpenFiles(){}
    OpenFiles::~OpenFiles(){}

    int OpenFiles::add(boost::filesystem::path volumeFile, Springy::Volume::IVolume *volume, int internalFd, int flags, mode_t mode){
        Synchronized syncOpenFiles(this->openFiles);

        int fd = 0;

        int *syncToken = NULL;
        openFiles_set::index<of_idx_volumeFile>::type &idx = this->openFiles.get<of_idx_volumeFile>();
        openFiles_set::index<of_idx_volumeFile>::type::iterator it = idx.find(volumeFile);
        if (it != idx.end()) {
            syncToken = it->o.syncToken;
        } else {
            syncToken = new int;
        }

        openFiles_set::index<of_idx_fd>::type &fdidx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator fdit = fdidx.begin();
        for(;fdit!=fdidx.end();fdit++, fd++){
            if(fdit->o.fd != fd){
                break;
            }
        }

        OpenFiles::openFile of;
        of.volumeFile = volumeFile;
        of.volume = volume;
        of.fd = internalFd;
        of.flags = flags;
        of.syncToken = syncToken;

        openFileSetEntry ofse;
        ofse.o         = of;
        ofse.fd        = fd;
        ofse.valid     = true;

        this->openFiles.insert(ofse);

        return fd;
    }
    OpenFiles::openFile OpenFiles::getByDescriptor(int fd){
        Synchronized syncOpenFiles(this->openFiles);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if (it == idx.end()) {
            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "bad descriptor";
        }
        return it->o;
    }
    void OpenFiles::remove(int fd){
        Synchronized syncOpenFiles(this->openFiles);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if (it == idx.end()) {
            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "bad descriptor";
        }

        int *syncToken = it->o.syncToken;
        boost::filesystem::path volumeFile = it->o.volumeFile;
        idx.erase(it);

        openFiles_set::index<of_idx_volumeFile>::type &vidx = this->openFiles.get<of_idx_volumeFile>();
        if (vidx.find(volumeFile) == vidx.end()) {
            delete syncToken;
        }
    }
}