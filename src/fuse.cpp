#ifdef HAS_FUSE

#include "fuse.hpp"

#include "util/synchronized.hpp"
#include "exception.hpp"
#include "trace.hpp"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <dirent.h>

#include <set>
#include <map>
#include <sstream>
#include <boost/algorithm/string/join.hpp>
#include <boost/exception/diagnostic_information.hpp>

namespace Springy {

    const char *Fuse::fsname = "springy";

    Fuse::Fuse() {
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        this->fuse = NULL;
        this->config = NULL;
        this->libc = NULL;
        this->readonly = false;
        this->withinTearDown = false;
    }

    Fuse& Fuse::init(Springy::Settings *config, Springy::LibC::ILibC *libc) {
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        if (config == NULL || libc == NULL) {
            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "invalid argument given ";
        }

        this->config = config;
        this->libc = libc;
        
        this->operations = new Springy::FsOps::Fuse(config, libc);

        this->fops.init = Fuse::init;
        this->fops.destroy = Fuse::destroy;

        this->fops.getattr = Fuse::getattr;
        this->fops.statfs = Fuse::statfs;
        this->fops.readdir = Fuse::readdir;
        this->fops.readlink = Fuse::readlink;

        this->fops.open = Fuse::open;
        this->fops.create = Fuse::create;
        this->fops.release = Fuse::release;

        this->fops.read = Fuse::read;
        this->fops.write = Fuse::write;
        this->fops.truncate = Fuse::truncate;
        this->fops.ftruncate = Fuse::ftruncate;
        this->fops.access = Fuse::access;
        this->fops.mkdir = Fuse::mkdir;
        this->fops.rmdir = Fuse::rmdir;
        this->fops.unlink = Fuse::unlink;
        this->fops.rename = Fuse::rename;
        this->fops.utimens = Fuse::utimens;
        this->fops.chmod = Fuse::chmod;
        this->fops.chown = Fuse::chown;
        this->fops.symlink = Fuse::symlink;
        this->fops.mknod = Fuse::mknod;
        this->fops.fsync = Fuse::fsync;
        this->fops.link = Fuse::link;

        this->fops.lock = Fuse::lock;

#ifndef WITHOUT_XATTR
        this->fops.setxattr = Fuse::setxattr;
        this->fops.getxattr = Fuse::getxattr;
        this->fops.listxattr = Fuse::listxattr;
        this->fops.removexattr = Fuse::removexattr;
#endif

        //int(* 	opendir )(const char *, struct fuse_file_info *)
        //int(* 	releasedir )(const char *, struct fuse_file_info *)
        //int(* 	fsyncdir )(const char *, int, struct fuse_file_info *)
        //int(* 	fgetattr )(const char *, struct stat *, struct fuse_file_info *)
        //int(* 	lock )(const char *, struct fuse_file_info *, int cmd, struct flock *)
        //int(* 	bmap )(const char *, size_t blocksize, uint64_t *idx)
        //int(* 	ioctl )(const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data)
        //int(* 	poll )(const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp)
        //int(* 	write_buf )(const char *, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *)
        //int(* 	read_buf )(const char *, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *)
        //int(* 	flock )(const char *, struct fuse_file_info *, int op)
        //int(* 	fallocate )(const char *, int, off_t, off_t, struct fuse_file_info *)

        return *this;
    }

    Fuse& Fuse::setUp(bool singleThreaded) {
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        this->mountpoint = this->config->mountpoint;

        this->singleThreaded = singleThreaded;

        this->fmountpoint = NULL; // the mountpoint is encoded within fuseArgv!
        int multithreaded = (int) this->singleThreaded;

        std::vector<const char*> fuseArgv;
        fuseArgv.push_back(this->fsname);
        fuseArgv.push_back(this->mountpoint.c_str());
        if (this->singleThreaded) {
            fuseArgv.push_back("-s");
        }
        fuseArgv.push_back("-f");

        this->readonly = (this->config->options.find("ro") != this->config->options.end());

        this->fuseoptions = boost::algorithm::join(this->config->options, ",");
        if (this->fuseoptions.size() > 0) {
            fuseArgv.push_back("-o");
            fuseArgv.push_back(this->fuseoptions.c_str());
        }

        this->fuse = fuse_setup(fuseArgv.size(), (char**) &fuseArgv[0], &this->fops, sizeof (this->fops),
                &this->fmountpoint, &multithreaded, static_cast<void*> (this));
        if (this->fuse == NULL) {
            // fuse failed!
            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "initializing fuse failed";
        }

        return *this;
    }

    Fuse& Fuse::run() {
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        this->th = std::thread(Fuse::thread, this);

        return *this;
    }

    Fuse& Fuse::tearDown() {
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        this->withinTearDown = true;

        if (this->fuse && !fuse_exited(this->fuse)) {
            fuse_exit(this->fuse);
        }

        if (this->th.joinable()) {
            // stat is needed cause Fuse::thread doesnt receive any signal's and thus
            // fuse won't exit the loop in case the exit is called
            // (details about this are explained here: https://stackoverflow.com/questions/8903448/libfuse-exiting-fuse-session-loop)
            // the reason why Fuse::thread shall not receive signal's is that
            // it provides more control about how to tearDown fuse, e.g. by a signal handler
            // or during runtime or maybe after an unhandled exception
            // the price to pay is that read(internal_fuse_fd) is blocking and the following stat
            // causes an event which allow's above's fuse_exit to do its job
            // the Fuse::thread method then calls fuse_teardown
            struct stat buf;
            this->libc->stat(__LINE__, this->mountpoint.c_str(), &buf);

            // wait for
            try {
                this->th.join();
            } catch (...) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            }
        }

        this->fuse = NULL;
        this->fmountpoint = NULL;

        delete this->operations;
        this->operations = NULL;

        this->withinTearDown = false;

        return *this;
    }

    void Fuse::thread(Fuse *instance) {
        std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;

        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        sigset_t set;
        // Block all signals in fuse thread - so all signals are delivered to another (main) thread
        sigemptyset(&set);
        sigfillset(&set);
        pthread_sigmask(SIG_SETMASK, &set, NULL);

        if (instance->singleThreaded) {
            fuse_loop_mt(instance->fuse);
        } else {
            fuse_loop(instance->fuse);
        }

        if (instance->fuse != NULL) {
            fuse_teardown(instance->fuse, instance->fmountpoint);
        }
    }

    void Fuse::determineCaller(uid_t *u, gid_t *g, pid_t *p, mode_t *mask) {
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        if (ctx == NULL) return;
        if (u != NULL) *u = ctx->uid;
        if (g != NULL) *g = ctx->gid;
        if (p != NULL) *p = ctx->pid;
        if (mask != NULL) *mask = ctx->umask;
    }

    Fuse::~Fuse() {
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }

    ////// static functions to forward function call to Fuse* instance

    void* Fuse::init(struct fuse_conn_info *conn) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        return instance;
    }

    void Fuse::destroy(void *arg) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }

    int Fuse::getattr(const char *path, struct stat *buf) {
        std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }

        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);

        return instance->operations->getattr(meta, boost::filesystem::path(path), buf);
    }

    int Fuse::statfs(const char *path, struct statvfs *buf) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }

        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);

        return instance->operations->statfs(meta, path, buf);
    }

    int Fuse::readdir(const char *dirname, void *buf, fuse_fill_dir_t filler,
            off_t offset, struct fuse_file_info * fi) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }

        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        std::unordered_map<std::string, struct stat> directories;

        int rval = instance->operations->readdir(meta, boost::filesystem::path(dirname), buf, offset, directories);
        
        if(rval == 0){
            std::unordered_map<std::string, struct stat>::iterator dit;
            for (dit = directories.begin(); dit != directories.end(); dit++) {
                if (filler(buf, dit->first.c_str(), &dit->second, 0))
                    break;
            }
        }

        return rval;
    }

    int Fuse::readlink(const char *path, char *buf, size_t size) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }

        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);

        return instance->operations->readlink(meta, boost::filesystem::path(path), buf, size);
    }

    int Fuse::create(const char *path, mode_t mode, struct fuse_file_info *fi) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->create(meta, boost::filesystem::path(path), mode, fi);
    }

    int Fuse::open(const char *path, struct fuse_file_info *fi) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);

        return instance->operations->open(meta, boost::filesystem::path(path), fi->fh);
    }

    int Fuse::release(const char *path, struct fuse_file_info *fi) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->open(meta, boost::filesystem::path(path), fi);
    }

    int Fuse::read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->read(meta, path, buf, count, offset, fi);
    }

    int Fuse::write(const char *path, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->write(meta, boost::filesystem::path(path), buf, count, offset, fi);
    }

    int Fuse::truncate(const char *path, off_t size) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->truncate(meta, boost::filesystem::path(path), size);
    }

    int Fuse::ftruncate(const char *path, off_t size, struct fuse_file_info *fi) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);

        return instance->operations->ftruncate(meta, boost::filesystem::path(path), size, fi);
    }

    int Fuse::access(const char *path, int mask) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->access(meta, boost::filesystem::path(path), mask);
    }

    int Fuse::mkdir(const char *path, mode_t mode) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->mkdir(meta, boost::filesystem::path(path), mode);
    }

    int Fuse::rmdir(const char *path) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->rmdir(meta, path);
    }

    int Fuse::unlink(const char *path) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->unlink(meta, path);
    }

    int Fuse::rename(const char *from, const char *to) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->rename(meta, boost::filesystem::path(from), boost::filesystem::path(to));
    }

    int Fuse::utimens(const char *path, const struct timespec ts[2]) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->utimens(meta, boost::filesystem::path(path), ts);
    }

    int Fuse::chmod(const char *path, mode_t mode) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->chmod(meta, boost::filesystem::path(path), mode);
    }

    int Fuse::chown(const char *path, uid_t uid, gid_t gid) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->chown(meta, boost::filesystem::path(path), uid, gid);
    }

    int Fuse::symlink(const char *from, const char *to) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->symlink(meta, boost::filesystem::path(from), boost::filesystem::path(to));
    }

    int Fuse::link(const char *from, const char *to) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->link(meta, boost::filesystem::path(from), boost::filesystem::path(to));
    }

    int Fuse::mknod(const char *path, mode_t mode, dev_t rdev) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->mknod(meta, boost::filesystem::path(path), mode, rdev);
    }

    int Fuse::fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->fsync(meta, boost::filesystem::path(path), isdatasync, fi);
    }

    int Fuse::lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lck) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->lock(meta, boost::filesystem::path(path), fi->fh, cmd, lck, &fi->lock_owner, sizeof(fi->lock_owner));
    }

    int Fuse::setxattr(const char *path, const char *attrname,
            const char *attrval, size_t attrvalsize, int flags) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->setxattr(meta, boost::filesystem::path(path), attrname, attrval, attrvalsize, flags);
    }

    int Fuse::getxattr(const char *path, const char *attrname, char *buf, size_t count) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->getxattr(meta, boost::filesystem::path(path), attrname, buf, count);
    }

    int Fuse::listxattr(const char *path, char *buf, size_t count) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->listxattr(meta, boost::filesystem::path(path), buf, count);
    }

    int Fuse::removexattr(const char *path, const char *attrname) {
        //std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
        Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        struct fuse_context *ctx = fuse_get_context();
        Fuse *instance = static_cast<Fuse*> (ctx->private_data);
        if (instance->withinTearDown) {
            return -ENOENT;
        }
        
        Springy::FsOps::Abstract::MetaRequest meta;
        meta.readonly = instance->readonly;
        instance->determineCaller(&meta.u, &meta.g, &meta.p, &meta.mask);
        
        return instance->operations->removexattr(meta, boost::filesystem::path(path), attrname);
    }


}

#endif
