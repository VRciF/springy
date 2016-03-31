/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   abstract.hpp
 * Author: user
 *
 * Created on March 26, 2016, 8:02 PM
 */

#ifndef SPRINGY_FSOPS_ABSTRACT_HPP
#define SPRINGY_FSOPS_ABSTRACT_HPP

#include <boost/filesystem.hpp>

#include <fuse.h>

#include "../volume/ivolume.hpp"
#include "../settings.hpp"
#include "../libc/ilibc.hpp"

namespace Springy{
    namespace FsOps{
        class Abstract{
            public:
            
            protected:
                struct VolumeInfo{
                    boost::filesystem::path virtualMountPoint;
                    boost::filesystem::path volumeRelativeFileName;
                    Springy::Volume::IVolume* volume;
                    struct stat st;
                    struct statvfs stvfs;
                };

                Springy::Settings *config;
                Springy::LibC::ILibC *libc;

                virtual VolumeInfo findVolume(const boost::filesystem::path file_name) = 0;
                virtual VolumeInfo getMaxFreeSpaceVolume(const boost::filesystem::path path) = 0;
                virtual Springy::Volumes::VolumeRelativeFile getVolumesByVirtualFileName(const boost::filesystem::path file_name) = 0;
                virtual int cloneParentDirsIntoVolume(Springy::Volume::IVolume *volume, const boost::filesystem::path path);

            public:
                Abstract(Springy::Settings *config, Springy::LibC::ILibC *libc);
                virtual ~Abstract();

                struct MetaRequest{
                    uid_t u;
                    gid_t g;
                    pid_t p;
                    mode_t mask;
                    bool readonly;
                };

                virtual int lock(MetaRequest meta, const boost::filesystem::path path, int fd, int cmd, struct ::flock *lck, const void *owner, size_t owner_len);

                virtual int getattr(MetaRequest meta, const boost::filesystem::path file_name, struct stat *buf);
                virtual int truncate(MetaRequest meta, const boost::filesystem::path path, off_t size);
                virtual int statfs(MetaRequest meta, const boost::filesystem::path path, struct statvfs *buf);
                virtual int readdir(MetaRequest meta, const boost::filesystem::path dirname, void *buf, off_t offset, std::unordered_map<std::string, struct stat> &directories);
                virtual int readlink(MetaRequest meta, const boost::filesystem::path path, char *buf, size_t size);
                virtual int access(MetaRequest meta, const boost::filesystem::path path, int mask);
                virtual int mkdir(MetaRequest meta, const boost::filesystem::path path, mode_t mode);
                virtual int rmdir(MetaRequest meta, const boost::filesystem::path path);
                virtual int unlink(MetaRequest meta, const boost::filesystem::path path);
                virtual int rename(MetaRequest meta, const boost::filesystem::path from, const boost::filesystem::path to);
                virtual int utimens(MetaRequest meta, const boost::filesystem::path path, const struct timespec ts[2]);
                virtual int chmod(MetaRequest meta, const boost::filesystem::path path, mode_t mode);
                virtual int chown(MetaRequest meta, const boost::filesystem::path path, uid_t uid, gid_t gid);
                virtual int symlink(MetaRequest meta, const boost::filesystem::path from, const boost::filesystem::path to);
                virtual int link(MetaRequest meta, const boost::filesystem::path from, const boost::filesystem::path to);
                virtual int mknod(MetaRequest meta, const boost::filesystem::path path, mode_t mode, dev_t rdev);

                virtual int setxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname,
                                     const char *attrval, size_t attrvalsize, int flags);
                virtual int getxattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname, char *buf, size_t count);
                virtual int listxattr(MetaRequest meta, const boost::filesystem::path file_name, char *buf, size_t count);
                virtual int removexattr(MetaRequest meta, const boost::filesystem::path file_name, const std::string attrname);

                virtual int create(MetaRequest meta, const boost::filesystem::path file, mode_t mode, struct ::fuse_file_info *fi);
                virtual int open(MetaRequest meta, const boost::filesystem::path file, struct ::fuse_file_info *fi);
                virtual int release(MetaRequest meta, const boost::filesystem::path path, struct ::fuse_file_info *fi);
                virtual int read(MetaRequest meta, const boost::filesystem::path file, char *buf, size_t count, off_t offset, struct ::fuse_file_info *fi);
                virtual int write(MetaRequest meta, const boost::filesystem::path file, const char *buf, size_t count, off_t offset, struct ::fuse_file_info *fi);
                virtual int ftruncate(MetaRequest meta, const boost::filesystem::path path, off_t size, struct ::fuse_file_info *fi);
                virtual int fsync(MetaRequest meta, const boost::filesystem::path path, int isdatasync, struct ::fuse_file_info *fi);
        };
    }
}

#endif /* ABSTRACT_HPP */

