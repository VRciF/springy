#include "abstract.hpp"

#include <errno.h>
#include <boost/filesystem/path.hpp>

namespace Springy {
    namespace FsOps {
        Abstract::Abstract(Springy::Settings *config, Springy::LibC::ILibC *libc){
            this->config = config;
            this->libc   = libc;
        }
        Abstract::~Abstract(){}

        int Abstract::cloneParentDirsIntoVolume(Springy::Volume::IVolume *volume, const boost::filesystem::path path) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            boost::filesystem::path parent = path.parent_path();
            if(parent.empty()){
                Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return 0;
            }

            struct stat st;
            // already exists
            if (volume->getattr(parent, &st) == 0) {
                if (!S_ISDIR(st.st_mode)) {
                    errno = ENOTDIR;
                    Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }
                return 0;
            }

            VolumeInfo vinfo;
            try {
                vinfo = this->findVolume(parent);
            } catch (...) {
                Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -EFAULT;
            }

            // create parent dirs
            int res = this->cloneParentDirsIntoVolume(volume, parent);
            if (res != 0) {
                return res;
            }

            res = volume->mkdir(parent, st.st_mode);
            if (res == 0) {
                volume->chown(parent, st.st_uid, st.st_gid);
                volume->chmod(parent, st.st_mode);
//#ifndef WITHOUT_XATTR
                // copy extended attributes of parent dir
//                this->copy_xattrs(vinfo.volume, volume, vinfo.volumeRelativeFileName);
//#endif
            } else {
                //Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__) << volume->string() << " : " << parent.string();
                Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__) << (volume->string()) << " : " << parent.string();
                res = -errno;
            }

            return res;
        }

        int Abstract::lock(MetaRequest meta, const boost::filesystem::path path, int fd, int cmd, struct ::flock *lck, const void *owner, size_t owner_len){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            try {
                VolumeInfo vinfo = this->findVolume(path);
                int res = vinfo.volume->lock(path, fd, cmd, lck, (const uint64_t*)owner);
                if (res == -1)
                    return -errno;
            } catch (...) {
                errno = EBADFD;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            return 0;
        }

        /////////////////// Path based operations ////////////////////////////////

        int Abstract::getattr(MetaRequest meta, const boost::filesystem::path file_name, struct stat *buf){
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (buf == nullptr) {
                return -EINVAL;
            }

            try {
                VolumeInfo vinfo = this->findVolume(file_name);
                *buf = vinfo.st;
                return 0;
            } catch (...) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            }

            return -ENOENT;
        }
        

        int Abstract::truncate(MetaRequest meta, const boost::filesystem::path path, off_t size) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            try {
                VolumeInfo vinfo = this->findVolume(path);
                int res = vinfo.volume->truncate(vinfo.volumeRelativeFileName, -1, size);

                if (res == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }
                return 0;
            } catch (...) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            }
            errno = ENOENT;
            return -errno;
        }

        int Abstract::statfs(MetaRequest meta, const boost::filesystem::path path, struct statvfs *buf) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (buf == nullptr) {
                return -EINVAL;
            }

            std::vector<struct statvfs> stats;
            std::set<dev_t> localDevices;

            try {
                this->findVolume(path);
            } catch (...) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -ENOENT;
            }

            unsigned long min_block = 0, min_frame = 0;

            Springy::Volumes::VolumeRelativeFile vols = this->config->volumes.getVolumesByVirtualFileName(path);

            std::set<Springy::Volume::IVolume*>::iterator it;
            for (it = vols.volumes.begin(); it != vols.volumes.end(); it++) {
                Springy::Volume::IVolume *volume = *it;
                if (volume->isLocal()) {
                    struct stat st;
                    int ret = volume->getattr(vols.volumeRelativeFileName, &st);
                    if (ret != 0) {
                        return -errno;
                    }
                    if (localDevices.find(st.st_dev) == localDevices.end()) {
                        continue;
                    }
                }

                struct statvfs stv;
                int ret = volume->statvfs(vols.volumeRelativeFileName, &stv);
                if (ret != 0) {
                    return -errno;
                }

                stats.push_back(stv);

                if (stats.size() == 1) {
                    min_block = stv.f_bsize,
                            min_frame = stv.f_frsize;
                } else {
                    if (min_block > stv.f_bsize) min_block = stv.f_bsize;
                    if (min_frame > stv.f_frsize) min_frame = stv.f_frsize;
                }
            }

            if (!min_block)
                min_block = 512;
            if (!min_frame)
                min_frame = 512;

            for (unsigned int i = 0; i < stats.size(); i++) {
                if (stats[i].f_bsize > min_block) {
                    stats[i].f_bfree *= stats[i].f_bsize / min_block;
                    stats[i].f_bavail *= stats[i].f_bsize / min_block;
                    stats[i].f_bsize = min_block;
                }
                if (stats[i].f_frsize > min_frame) {
                    stats[i].f_blocks *= stats[i].f_frsize / min_frame;
                    stats[i].f_frsize = min_frame;
                }

                if (i == 0) {
                    memcpy(buf, &stats[i], sizeof (struct statvfs));
                    continue;
                }

                if (buf->f_namemax < stats[i].f_namemax) {
                    buf->f_namemax = stats[i].f_namemax;
                }
                buf->f_ffree += stats[i].f_ffree;
                buf->f_files += stats[i].f_files;
                buf->f_favail += stats[i].f_favail;
                buf->f_bavail += stats[i].f_bavail;
                buf->f_bfree += stats[i].f_bfree;
                buf->f_blocks += stats[i].f_blocks;
            }

            return 0;
        }

        int Abstract::readdir(
                MetaRequest meta,
                const boost::filesystem::path dirname,
                void *buf,
                off_t offset,
                std::unordered_map<std::string, struct stat> &directories) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            struct stat st;
            size_t found = 0;
            std::vector<Springy::Volume::IVolume*> dirs;

            Springy::Volumes::VolumeRelativeFile vols = this->config->volumes.getVolumesByVirtualFileName(dirname);

            std::set<Springy::Volume::IVolume*>::iterator it;
            for (it = vols.volumes.begin(); it != vols.volumes.end(); it++) {
                Springy::Volume::IVolume *volume = *it;

                // check if the volume actually has the given dirname
                if (volume->getattr(vols.volumeRelativeFileName, &st) == 0) {
                    found++;
                    if (S_ISDIR(st.st_mode)) {
                        dirs.push_back(volume);
                        continue;
                    }
                }
            }

            // dirs not found
            if (dirs.size() <= 0) {
                errno = ENOENT;
                if (found) errno = ENOTDIR;

                return -errno;
            }

            // read directories
            for (size_t i = 0; i < dirs.size(); i++) {
                Springy::Volume::IVolume *volume = dirs[i];
                volume->readdir(vols.volumeRelativeFileName, directories);
            }

            return 0;
        }

        int Abstract::readlink(MetaRequest meta, const boost::filesystem::path path, char *buf, size_t size) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (buf == NULL) {
                return -EINVAL;
            }

            int res = 0;
            try {
                VolumeInfo vinfo = this->findVolume(path);
                memset(buf, 0, size);
                res = vinfo.volume->readlink(vinfo.volumeRelativeFileName, buf, size);

                if (res >= 0)
                    return 0;
                return -errno;
            } catch (...) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            }
            return -ENOENT;
        }

        int Abstract::access(MetaRequest meta, const boost::filesystem::path path, int mode) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly && (mode & F_OK) != F_OK && (mode & R_OK) != R_OK) {
                return -EROFS;
            }

            try {
                VolumeInfo vinfo = this->findVolume(path);
                int res = vinfo.volume->access(vinfo.volumeRelativeFileName, mode);

                if (res == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }
                return 0;
            } catch (...) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            }
            errno = ENOENT;
            return -errno;
        }

        int Abstract::mkdir(MetaRequest meta, const boost::filesystem::path path, mode_t mode) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            VolumeInfo vinfo;
            try {
                vinfo = this->findVolume(path);

                errno = EEXIST;
                return -errno;
            } catch (...) {
            }

            try {
                boost::filesystem::path parent = path.parent_path();
                if(parent.empty()){ throw ENOENT; }
                this->findVolume(parent);
            } catch (...) {
                errno = ENOENT;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            try {
                vinfo = this->getMaxFreeSpaceVolume(path);
            } catch (...) {
                errno = ENOSPC;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            this->cloneParentDirsIntoVolume(vinfo.volume, vinfo.volumeRelativeFileName);
            if (vinfo.volume->mkdir(vinfo.volumeRelativeFileName, mode) == 0) {
                if (this->libc->getuid(__LINE__) == 0) {
                    struct stat st;
                    gid_t gid = meta.g;
                    if (vinfo.volume->getattr(vinfo.volumeRelativeFileName, &st) == 0) {
                        // parent directory is SGID'ed
                        if (st.st_gid != this->libc->getgid(__LINE__))
                            gid = st.st_gid;
                    }
                    vinfo.volume->chown(vinfo.volumeRelativeFileName, meta.u, gid);
                }
                return 0;
            }

            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        int Abstract::rmdir(MetaRequest meta, const boost::filesystem::path path) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            try {
                VolumeInfo vinfo = this->findVolume(path);
                int res = vinfo.volume->rmdir(vinfo.volumeRelativeFileName);
                if (res == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }

                return 0;
            } catch (...) {
            }

            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -ENOENT;
        }

        int Abstract::unlink(MetaRequest meta, const boost::filesystem::path path) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            try {
                VolumeInfo vinfo = this->findVolume(path);
                int res = vinfo.volume->unlink(vinfo.volumeRelativeFileName);

                if (res == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }

                return 0;
            } catch (...) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                errno = ENOENT;
                return -errno;
            }
        }

        int Abstract::rename(MetaRequest meta, const boost::filesystem::path from, const boost::filesystem::path to) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            int res;
            struct stat st;

            if (from == to)
                return 0;

            boost::filesystem::path fromParent = from.parent_path();
            if(fromParent.empty()){ fromParent = boost::filesystem::path("/"); }
            boost::filesystem::path toParent = to.parent_path();
            if(toParent.empty()){ toParent = boost::filesystem::path("/"); }

            Springy::Volumes::VolumeRelativeFile fromVolumes = this->config->volumes.getVolumesByVirtualFileName(from);

            std::set<Springy::Volume::IVolume*>::iterator it;
            for (it = fromVolumes.volumes.begin(); it != fromVolumes.volumes.end(); it++) {
                if ((*it)->getattr(fromVolumes.volumeRelativeFileName, &st) != 0) {
                    continue;
                }

                res = (*it)->rename(fromVolumes.volumeRelativeFileName, to);
                if (res == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }
            }

            return 0;
        }

        int Abstract::utimens(MetaRequest meta, const boost::filesystem::path path, const struct timespec ts[2]) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            size_t flag_found = 0;
            int res;
            struct stat st;

            Springy::Volumes::VolumeRelativeFile pathVolumes = this->config->volumes.getVolumesByVirtualFileName(path);

            std::set<Springy::Volume::IVolume*>::iterator it;
            for (it = pathVolumes.volumes.begin(); it != pathVolumes.volumes.end(); it++) {
                if ((*it)->getattr(pathVolumes.volumeRelativeFileName, &st) != 0) {
                    continue;
                }

                flag_found = 1;

                res = (*it)->utimensat(pathVolumes.volumeRelativeFileName, ts);
                if (res == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }
            }
            if (flag_found)
                return 0;
            errno = ENOENT;
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        int Abstract::chmod(MetaRequest meta, const boost::filesystem::path path, mode_t mode) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            size_t flag_found;
            int res;
            struct stat st;

            Springy::Volumes::VolumeRelativeFile pathVolumes = this->config->volumes.getVolumesByVirtualFileName(path);

            std::set<Springy::Volume::IVolume*>::iterator it;
            for (it = pathVolumes.volumes.begin(); it != pathVolumes.volumes.end(); it++) {
                if ((*it)->getattr(pathVolumes.volumeRelativeFileName, &st) != 0) {
                    continue;
                }

                flag_found = 1;
                res = (*it)->chmod(pathVolumes.volumeRelativeFileName, mode);

                if (res == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }
            }
            if (flag_found)
                return 0;
            errno = ENOENT;
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        int Abstract::chown(MetaRequest meta, const boost::filesystem::path path, uid_t uid, gid_t gid) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            size_t flag_found;
            int res;
            struct stat st;

            Springy::Volumes::VolumeRelativeFile pathVolumes = this->config->volumes.getVolumesByVirtualFileName(path);

            std::set<Springy::Volume::IVolume*>::iterator it;
            for (it = pathVolumes.volumes.begin(); it != pathVolumes.volumes.end(); it++) {
                if ((*it)->getattr(pathVolumes.volumeRelativeFileName, &st) != 0) {
                    continue;
                }

                flag_found = 1;
                res = (*it)->chown(pathVolumes.volumeRelativeFileName, uid, gid);

                if (res == -1) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }
            }
            if (flag_found)
                return 0;
            errno = ENOENT;
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        int Abstract::symlink(MetaRequest meta, const boost::filesystem::path oldname, const boost::filesystem::path newname) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            int res;

            // symlink only works on same volume type
            VolumeInfo vinfo;
            boost::filesystem::path parent = newname.parent_path();
            try {
                if(parent.empty()){ throw ENOENT; }
                vinfo = this->findVolume(parent);
            } catch (...) {
                errno = ENOENT;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            // symlink into found Volume
            res = vinfo.volume->symlink(this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, oldname),
                    this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, newname));
            if (res == 0) {
                return 0;
            }
            if (errno != ENOSPC) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            try {
                vinfo = this->getMaxFreeSpaceVolume(parent);
            } catch (...) {
                errno = ENOSPC;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            // symlink into max free space volume
            this->cloneParentDirsIntoVolume(vinfo.volume, this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, newname));
            res = vinfo.volume->symlink(this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, oldname),
                    this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, newname));
            if (res == 0) {
                return 0;
            }
            if (errno != ENOSPC) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        int Abstract::link(MetaRequest meta, const boost::filesystem::path oldname, const boost::filesystem::path newname) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            int res = 0;
            VolumeInfo vinfo;

            try {
                vinfo = this->findVolume(oldname);
            } catch (...) {
                errno = ENOENT;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            res = this->cloneParentDirsIntoVolume(vinfo.volume, this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, newname));
            if (res != 0) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return res;
            }

            res = vinfo.volume->link(this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, oldname),
                    this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, newname));

            if (res == 0)
                return 0;
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        int Abstract::mknod(MetaRequest meta, const boost::filesystem::path path, mode_t mode, dev_t rdev) {
            Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            if (meta.readonly) {
                return -EROFS;
            }

            int res, i;
            boost::filesystem::path parent = path.parent_path();
            VolumeInfo vinfo;
            try {
                if(parent.empty()){ throw ENOENT; }
                vinfo = this->findVolume(parent);
            } catch (...) {
                errno = ENOENT;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -errno;
            }

            for (i = 0; i < 2; i++) {
                if (i) {
                    try {
                        vinfo = this->getMaxFreeSpaceVolume(parent);
                    } catch (...) {
                        errno = ENOSPC;
                        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                        return -errno;
                    }

                    this->cloneParentDirsIntoVolume(vinfo.volume, this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, path));
                }

                if (S_ISREG(mode)) {
                    res = vinfo.volume->open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
                    if (res >= 0)
                        res = vinfo.volume->close(path, res);
                } else if (S_ISFIFO(mode))
                    res = vinfo.volume->mkfifo(path, mode);
                else
                    res = vinfo.volume->mknod(path, mode, rdev);

                if (res != -1) {
                    if (this->libc->getuid(__LINE__) == 0) {
                        vinfo.volume->chown(path, meta.u, meta.g);
                    }

                    return 0;
                }

                if (errno != ENOSPC) {
                    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
                    return -errno;
                }
            }
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }


        //#ifndef WITHOUT_XATTR
        //int Abstract::setxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname,
        //                const char *attrval, size_t attrvalsize, int flags){
        //    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //
        //	try{
        //		boost::filesystem::path path = this->findVolume(file_name);
        //        if (this->libc->setxattr(__LINE__, path.c_str(), attrname.c_str(), attrval, attrvalsize, flags) == -1){
        //            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //            return -errno;
        //         }
        //		return 0;
        //	}
        //	catch(...){
        //        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //    }
        //	return -ENOENT;
        //}

        //int Abstract::getxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname, char *buf, size_t count){
        //    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //
        //	try{
        //		boost::filesystem::path path = this->findPath(file_name);
        //        int size = this->libc->getxattr(__LINE__, path.c_str(), attrname.c_str(), buf, count);
        //        if(size == -1){
        //            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //            return -errno;
        //        }
        //		return size;
        //	}
        //	catch(...){
        //        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //    }
        //	return -ENOENT;
        //}

        //int Abstract::listxattr(MetaRequest meta, const boost::filesystem::path file_name, char *buf, size_t count){
        //    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //
        //    try{
        //		boost::filesystem::path path = this->findPath(file_name);
        //		int ret = 0;
        //        if((ret=this->libc->listxattr(__LINE__, path.c_str(), buf, count)) == -1){
        //            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //            return -errno;
        //        }
        //		return ret;
        //	}
        //	catch(...){
        //        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //    }
        //	return -ENOENT;
        //}

        //int Abstract::removexattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname){
        //    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //
        //	try{
        //		boost::filesystem::path path = this->findPath(file_name);
        //        if(this->libc->removexattr(__LINE__, path.c_str(), attrname.c_str()) == -1){
        //            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //            return -errno;
        //        }
        //		return 0;
        //	}
        //	catch(...){
        //        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        //    }
        //	return -ENOENT;
        //}
        //#endif
    }
}