#include "fuse.hpp"

namespace Springy {
    namespace FsOps {
        
        Fuse::Fuse(Springy::Settings *config, Springy::LibC::ILibC *libc) : Abstract(config, libc){}
        Fuse::~Fuse(){}

        Abstract::VolumeInfo Fuse::findVolume(const boost::filesystem::path file_name) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            Fuse::VolumeInfo vinfo;

            Springy::Volumes::VolumeRelativeFile vols = this->config->volumes.getVolumesByVirtualFileName(file_name);
            vinfo.virtualMountPoint = vols.virtualMountPoint;
            vinfo.volumeRelativeFileName = vols.volumeRelativeFileName;

            std::set<Springy::Volume::IVolume*>::iterator it;
            for (it = vols.volumes.begin(); it != vols.volumes.end(); it++) {
                Springy::Volume::IVolume *volume = *it;
                if (volume->getattr(vols.volumeRelativeFileName, &vinfo.st) != -1) {
                    vinfo.volume = volume;
                    volume->statvfs(vols.volumeRelativeFileName, &vinfo.stvfs);
                    //vinfo.curspace = vinfo.buf.f_bsize * vinfo.buf.f_bavail;

                    return vinfo;
                }
            }

            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "file not found";
        }

        Abstract::VolumeInfo Fuse::getMaxFreeSpaceVolume(const boost::filesystem::path path) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            Springy::Volumes::VolumeRelativeFile vols = this->config->volumes.getVolumesByVirtualFileName(path);

            Fuse::VolumeInfo vinfo;
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

                if (maxFreeSpaceVolume == NULL || curspace > space) {
                    vinfo.volume = volume;
                    vinfo.stvfs = stvfs;
                    volume->getattr(vols.volumeRelativeFileName, &vinfo.st);
                    maxFreeSpaceVolume = volume;

                    space = curspace;
                }
            }
            if (!maxFreeSpaceVolume) {
                return vinfo;
            }

            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "no space";
        }

        
        void Fuse::saveFd(boost::filesystem::path file, Springy::Volume::IVolume *volume, int fd, int flags) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            Synchronized syncOpenFiles(this->openFiles);

            int *syncToken = NULL;
            openFiles_set::index<of_idx_volumeFile>::type &idx = this->openFiles.get<of_idx_volumeFile>();
            openFiles_set::index<of_idx_volumeFile>::type::iterator it = idx.find(file);
            if (it != idx.end()) {
                syncToken = it->syncToken;
            } else {
                syncToken = new int;
            }

            struct openFile of = {file, volume, fd, flags, syncToken, true};
            this->openFiles.insert(of);
        }

        
/*
        int Fuse::copy_xattrs(Springy::Volume::IVolume *src, Springy::Volume::IVolume *dst, const boost::filesystem::path path) {
#ifndef WITHOUT_XATTR
                Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

                int listsize=0, attrvalsize=0;
                char *listbuf=NULL, *attrvalbuf=NULL,
             *name_begin=NULL, *name_end=NULL;

                // if not xattrs on source, then do nothing
                if ((listsize = src->listxattr(path, NULL, 0)) == 0)
                        return 0;

                // get all extended attributes
                listbuf=(char *)this->libc->calloc(__LINE__, sizeof(char), listsize);
                if (src->listxattr(path, listbuf, listsize) == -1)
                {
                    Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -1;
                }

                // loop through each xattr
                for(name_begin=listbuf, name_end=listbuf+1;
                        name_end < (listbuf + listsize); name_end++)
                {
                    // skip the loop if we're not at the end of an attribute name
                    if (*name_end != '\0')
                            continue;

                    // get the size of the extended attribute
                    attrvalsize = src->getxattr(path, name_begin, NULL, 0);
                    if (attrvalsize < 0)
                    {
                        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                        return -1;
                    }

                    // get the value of the extended attribute
                    attrvalbuf=(char *)this->libc->calloc(__LINE__, sizeof(char), attrvalsize);
                    if (src->getxattr(path, name_begin, attrvalbuf, attrvalsize) < 0)
                    {
                        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                        return -1;
                    }

                    // set the value of the extended attribute on dest file
                    if (dst->setxattr(path, name_begin, attrvalbuf, attrvalsize, 0) < 0)
                    {
                        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                        return -1;
                    }

                    this->libc->free(__LINE__, attrvalbuf);

                    // point the pointer to the start of the attr name to the start
                    // of the next attr
                    name_begin=name_end+1;
                    name_end++;
                }

                this->libc->free(__LINE__, listbuf);

#endif
            return 0;
        }

        void Fuse::reopen_files(const boost::filesystem::path file, const Springy::Volume::IVolume *volume) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                boost::filesystem::path newFile = this->concatPath(newDirectory, file);
                Synchronized syncOpenFiles(this->openFiles);

                openFiles_set::index<of_idx_fuseFile>::type &idx = this->openFiles.get<of_idx_fuseFile>();
                std::pair<openFiles_set::index<of_idx_fuseFile>::type::iterator,
                          openFiles_set::index<of_idx_fuseFile>::type::iterator> range = idx.equal_range(file);

                openFiles_set::index<of_idx_fuseFile>::type::iterator it;
                for(it = range.first;it!=range.second;it++){

                    off_t seek = this->libc->lseek(it->fd, 0, SEEK_CUR);
                    int flags = it->flags;
                    int fh;

                    flags &= ~(O_EXCL|O_TRUNC);

                    // open
                    if ((fh = this->libc->open(__LINE__, newFile.c_str(), flags)) == -1) {
                        it->valid = false;
                        continue;
                    }
                    else
                    {
                        // seek
                        if (seek != this->libc->lseek(__LINE__, fh, seek, SEEK_SET)) {
                            this->libc->close(__LINE__, fh);
                            it->valid = false;
                            continue;
                        }

                        // filehandle
                        if (this->libc->dup2(__LINE__, fh, it->fd) != it->fd) {
                            this->libc->close(__LINE__, fh);
                            it->valid = false;
                            continue;
                        }
                        // close temporary filehandle
                        this->libc->close(__LINE__, fh);
                    }

                    range.first->path = newDirectory;
                }
        }
*/
/*
        void Fuse::move_file(int fd, boost::filesystem::path file, Springy::Volume::IVolume *from, fsblkcnt_t wsize) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            char *buf = NULL;
            ssize_t size;
            FILE *input = NULL, *output = NULL;
            struct utimbuf ftime = {0};
            fsblkcnt_t space;
            struct stat st;

            Abstract::VolumeInfo to = this->getMaxFreeSpaceVolume(file);
            space = to.stvfs.f_bavail * to.stvfs.f_frsize;
            if(space<wsize || from == to.volume){
                throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "not enough space";
            }

            // get file size
            if (this->libc->fstat(__LINE__, fd, &st) != 0) {
            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "fstat failed";
            }

            // Hard link support is limited to a single device, and files with
            // >1 hardlinks cannot be moved between devices since this would
            // (a) result in partial files on the source device (b) not free
            // the space from the source device during unlink.
            if (st.st_nlink > 1) {
            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "cannot move file with hard links";
            }

            this->create_parent_dirs(to.volume, file);

            //boost::filesystem::path from = this->concatPath(directory, file);
            //boost::filesystem::path to = this->concatPath(maxSpaceDir, file);

            // in case fd is not open for reading - just open a new file pointer
            if (!(input = this->libc->fopen(__LINE__, from.c_str(), "r"))){
                    throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "open file for reading failed: " << from.string();
            }

            if (!(output = this->libc->fopen(__LINE__, to.c_str(), "w+"))) {
                this->libc->fclose(__LINE__, input);
                throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "open file for writing failed: " << to.string();
            }

            int inputfd = this->libc->fileno(__LINE__, input);
            int outputfd = this->libc->fileno(__LINE__, output);

                // move data
            uint32_t allocationSize = 16*1024*1024; // 16 megabyte
            buf = NULL;
            do{
                try{
                    buf = new char[allocationSize];
                    break;
                }catch(...){
                    allocationSize/=2;  // reduce allocation size at max to filesystem block size
                }
            }while(allocationSize>=st.st_blksize || allocationSize>=4096);
            if(buf==NULL){
                errno = ENOMEM;
                this->libc->fclose(__LINE__, input);
                this->libc->fclose(__LINE__, output);
                this->libc->unlink(__LINE__, to.c_str());
                throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "not enough memory";
            }

                while((size = this->libc->read(__LINE__, inputfd, buf, sizeof(char)*allocationSize))>0) {
                ssize_t written = 0;
                while(written<size){
                    size_t bytesWritten = this->libc->write(__LINE__, outputfd, buf+written, sizeof(char)*(size-written));
                    if(bytesWritten>0){
                        written += bytesWritten;
                    }
                    else{
                        this->libc->fclose(__LINE__, input);
                        this->libc->fclose(__LINE__, output);
                        delete[] buf;
                        this->libc->unlink(__LINE__, to.c_str());
                        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "moving file failed";
                    }
                }
                }

            delete[] buf;
                if(size==-1){
                this->libc->fclose(__LINE__, input);
                this->libc->fclose(__LINE__, output);
                this->libc->unlink(__LINE__, to.c_str());
                throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "read error occured";
            }

                this->libc->fclose(__LINE__, input);

                // owner/group/permissions
                this->libc->fchmod(__LINE__, outputfd, st.st_mode);
                this->libc->fchown(__LINE__, outputfd, st.st_uid, st.st_gid);
                this->libc->fclose(__LINE__, output);

                // time
                ftime.actime = st.st_atime;
                ftime.modtime = st.st_mtime;
                this->libc->utime(__LINE__, to.c_str(), &ftime);

        #ifndef WITHOUT_XATTR
                // extended attributes
                this->copy_xattrs(from, to, file);
        #endif

            try{
                Synchronized syncOpenFiles(this->openFiles);

                openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
                openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
                if(it!=idx.end()){
                    it->path = maxSpaceDir;  // path is not indexed, thus no need to replace in openFiles
                }
            }catch(...){
                this->libc->unlink(__LINE__, to.c_str());
                throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "failed to modify internal data structure";
            }

            try{
                this->reopen_files(file, maxSpaceDir);
                this->libc->unlink(__LINE__, from.c_str());
            }catch(...){
                this->libc->unlink(__LINE__, to.c_str());
                throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "failed to reopen already open files";
            }
        }
*/
        /////////////////// File descriptor operations ////////////////////////////////

        int Fuse::create(MetaRequest meta, const boost::filesystem::path file, mode_t mode, struct ::fuse_file_info *fi) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            int fd = Abstract::create(meta, file, mode, fi);
            if(fd >= 0){
                Abstract::VolumeInfo vinfo;
                try {
                    vinfo = this->findVolume(file);
                    this->saveFd(vinfo.volumeRelativeFileName, vinfo.volume, fd, O_CREAT | O_WRONLY | O_TRUNC);
                    fi->fh = fd;
                } catch (...) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    errno = ENOENT;
                    return -errno;
                }
            }

            return fd;
        }

        int Fuse::open(MetaRequest meta, const boost::filesystem::path file, struct ::fuse_file_info *fi) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            
            int fd = Abstract::open(meta, file, fi);
            if(fd >= 0){
                Abstract::VolumeInfo vinfo;
                try {
                    vinfo = this->findVolume(file);
                    this->saveFd(vinfo.volumeRelativeFileName, vinfo.volume, fd, O_CREAT | O_WRONLY | O_TRUNC);
                    fi->fh = fd;
                } catch (...) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    errno = ENOENT;
                    return -errno;
                }
            }

            return fd;
        }

        int Fuse::release(MetaRequest meta, const boost::filesystem::path path, struct ::fuse_file_info *fi) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            int fd = fi->fh;

            try {
                Synchronized syncOpenFiles(this->openFiles);

                openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
                openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
                if (it == idx.end()) {
                    throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "bad descriptor";
                }

                it->volume->close(path, fd);

                int *syncToken = it->syncToken;
                boost::filesystem::path volumeFile = it->volumeFile;
                idx.erase(it);

                openFiles_set::index<of_idx_volumeFile>::type &vidx = this->openFiles.get<of_idx_volumeFile>();
                if (vidx.find(volumeFile) == vidx.end()) {
                    delete syncToken;
                }
            } catch (...) {
                if (errno == 0) {
                    errno = EBADFD;
                }

                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);

                return -errno;
            }

            return 0;
        }

        int Fuse::read(MetaRequest meta, const boost::filesystem::path file, char *buf, size_t count, off_t offset, struct ::fuse_file_info *fi) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (buf == NULL) {
                return -EINVAL;
            }

            int fd = fi->fh;
            try {
                Synchronized syncOpenFiles(this->openFiles);

                openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
                openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
                if (it != idx.end() && it->valid == false) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "file descriptor has been invalidated internally");
                    errno = EINVAL;
                    return -errno;
                }

                Springy::Volume::IVolume *volume = it->volume;
                ssize_t res;
                res = volume->read(it->volumeFile, fd, buf, count, offset);
                if (res == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }

                return res;
            } catch (...) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                errno = EBADFD;
                return -errno;
            }
        }

        int Fuse::write(MetaRequest meta, const boost::filesystem::path file, const char *buf, size_t count, off_t offset, struct ::fuse_file_info *fi) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            if (buf == NULL) {
                return -EINVAL;
            }

            ssize_t res;
            int fd = fi->fh;

            int *syncToken = NULL;
            Springy::Volume::IVolume *volume;
            boost::filesystem::path volumeFile;

            try {
                Synchronized syncOpenFiles(this->openFiles);

                openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
                openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
                if (it != idx.end() && it->valid == false) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "file descriptor has been invalidated internally");
                    errno = EINVAL;
                    return -errno;
                }
                if (it == idx.end()) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "file descriptor not found");
                    errno = EBADFD;
                    return -errno;
                }

                syncToken  = it->syncToken;
                volume     = it->volume;
                volumeFile = it->volumeFile;
            } catch (...) {
                if (errno == 0) {
                    errno = EBADFD;
                }
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "exception catched");
                return -errno;
            }

            Synchronized sync(syncToken);

            errno = 0;
            res = volume->write(volumeFile, fd, buf, count, offset);
            if ((res >= 0) || (res == -1 && errno != ENOSPC)) {
                if (res == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }
                return res;
            }
            return -errno;

            //struct stat st;
            //volume->getattr(volumeFile, &st);

            //// write failed, cause of no space left
            //errno = ENOSPC;
            //try {
            //    this->move_file(fd, volumeFile, volume, (off_t) (offset + count) > st.st_size ? offset + count : st.st_size);
            //} catch (...) {
            //    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "exception catched");
            //    return -errno;
            //}

            //res = this->libc->pwrite(__LINE__, fd, buf, count, offset);
            //if (res == -1) {
            //    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "write failed");
            //    return -errno;
            //}

            //return res;
        }

        int Fuse::ftruncate(MetaRequest meta, const boost::filesystem::path path, off_t size, struct fuse_file_info *fi) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            int fd = fi->fh;
            try {
                Synchronized syncOpenFiles(this->openFiles);

                openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
                openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
                if (it != idx.end() && it->valid == false) {
                    // MISSING: log that the descriptor has been invalidated as the reason of failing
                    errno = EINVAL;
                    return -errno;
                }
                if (it->volume->truncate(it->volumeFile, fd, size) == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }
                return 0;
            } catch (...) {
            }

            errno = -EBADFD;
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        int Fuse::fsync(MetaRequest meta, const boost::filesystem::path path, int isdatasync, struct fuse_file_info *fi) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            int fd = fi->fh;
            try {
                Synchronized syncOpenFiles(this->openFiles);

                openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
                openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
                if (it != idx.end() && it->valid == false) {
                    errno = EINVAL;
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }

                int res = 0;
                res = it->volume->fsync(it->volumeFile, fd);

                if (res == -1)
                    return -errno;
            } catch (...) {
                errno = EBADFD;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            return 0;
        }

        
        int Fuse::lock(MetaRequest meta, const boost::filesystem::path path, int fd, int cmd, struct ::flock *lck, const void *owner, size_t owner_len){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            try {
                Synchronized syncOpenFiles(this->openFiles);

                openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
                openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
                if (it != idx.end() && it->valid == false) {
                    errno = EINVAL;
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }

                int res = it->volume->lock(it->volumeFile, fd, cmd, lck, (const uint64_t*)owner);
                if (res == -1)
                    return -errno;
            } catch (...) {
                errno = EBADFD;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            return 0;
        }

        int Fuse::setxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname,
                             const char *attrval, size_t attrvalsize, int flags){
            return -ENOTSUP;
        }
        int Fuse::getxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname, char *buf, size_t count){
            return -ENOTSUP;
        }
        int Fuse::listxattr(MetaRequest meta, const boost::filesystem::path file_name, char *buf, size_t count){
            return -ENOTSUP;
        }
        int Fuse::removexattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname){
            return -ENOTSUP;
        }
        
    }
}