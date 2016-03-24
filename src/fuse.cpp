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

namespace Springy{

const char *Fuse::fsname = "springy";

Fuse::Fuse(){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    this->fuse   = NULL;
    this->config = NULL;
    this->libc   = NULL;
    this->readonly = false;
}
Fuse& Fuse::init(Springy::Settings *config, Springy::LibC::ILibC *libc){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(config == NULL || libc == NULL){
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "invalid argument given ";
    }

    this->config = config;
    this->libc   = libc;

    this->fops.init	        = Fuse::init;
    this->fops.destroy      = Fuse::destroy;

    this->fops.getattr    	= Fuse::getattr;
    this->fops.statfs     	= Fuse::statfs;
    this->fops.readdir    	= Fuse::readdir;
    this->fops.readlink   	= Fuse::readlink;

    this->fops.open       	= Fuse::open;
    this->fops.create     	= Fuse::create;
    this->fops.release    	= Fuse::release;

    this->fops.read       	= Fuse::read;
    this->fops.write      	= Fuse::write;
    this->fops.truncate   	= Fuse::truncate;
    this->fops.ftruncate  	= Fuse::ftruncate;
    this->fops.access     	= Fuse::access;
    this->fops.mkdir      	= Fuse::mkdir;
    this->fops.rmdir      	= Fuse::rmdir;
    this->fops.unlink     	= Fuse::unlink;
    this->fops.rename     	= Fuse::rename;
    this->fops.utimens    	= Fuse::utimens;
    this->fops.chmod      	= Fuse::chmod;
    this->fops.chown      	= Fuse::chown;
    this->fops.symlink    	= Fuse::symlink;
    this->fops.mknod      	= Fuse::mknod;
    this->fops.fsync      	= Fuse::fsync;
    this->fops.link		    = Fuse::link;

    this->fops.lock		    = Fuse::lock;

#ifndef WITHOUT_XATTR
    this->fops.setxattr   	= Fuse::setxattr;
    this->fops.getxattr   	= Fuse::getxattr;
    this->fops.listxattr  	= Fuse::listxattr;
    this->fops.removexattr	= Fuse::removexattr;
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
Fuse& Fuse::setUp(bool singleThreaded){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    this->mountpoint = this->config->mountpoint;

    this->singleThreaded = singleThreaded;

    this->fmountpoint = NULL; // the mountpoint is encoded within fuseArgv!
    int multithreaded = (int)this->singleThreaded;

    std::vector<const char*> fuseArgv;
    fuseArgv.push_back(this->fsname);
    fuseArgv.push_back(this->mountpoint.c_str());
    if(this->singleThreaded){
        fuseArgv.push_back("-s");
    }
    fuseArgv.push_back("-f");

    this->readonly = (this->config->options.find("ro")!=this->config->options.end());

    this->fuseoptions = boost::algorithm::join(this->config->options, ",");
    if(this->fuseoptions.size()>0){
        fuseArgv.push_back("-o");
        fuseArgv.push_back(this->fuseoptions.c_str());
    }

    this->fuse = fuse_setup(fuseArgv.size(), (char**)&fuseArgv[0], &this->fops, sizeof(this->fops),
                      &this->fmountpoint, &multithreaded, static_cast<void*>(this));
    if(this->fuse==NULL){
        // fuse failed!
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "initializing fuse failed";
    }

    return *this;
}
Fuse& Fuse::run(){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    this->th = std::thread(Fuse::thread, this);

    return *this;
}
Fuse& Fuse::tearDown(){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->fuse && !fuse_exited(this->fuse)){
        fuse_teardown(this->fuse, this->fmountpoint);
        this->fmountpoint = NULL;
	}

    if(this->th.joinable()){
        struct stat buf;
        this->libc->stat(__LINE__, this->mountpoint.c_str(), &buf);

        // wait for 
        try{
            this->th.join();
        }catch(...){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        }
    }

    return *this;
}

void Fuse::thread(Fuse *instance){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    int res = 0;

	sigset_t set;
	// Block all signals in fuse thread - so all signals are delivered to another (main) thread
	sigemptyset(&set);
	sigfillset(&set);
	pthread_sigmask(SIG_SETMASK, &set, NULL);

	if (instance->singleThreaded){
		res = fuse_loop_mt(instance->fuse);
	}else{
		res = fuse_loop(instance->fuse);
	}

	fuse_teardown(instance->fuse, instance->fmountpoint);
	instance->fuse = NULL;
    if(res!=0){
        // force shutdown
    }
}

void Fuse::determineCaller(uid_t *u, gid_t *g, pid_t *p, mode_t *mask){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	struct fuse_context *ctx = fuse_get_context();
	if(ctx==NULL) return;
	if(u!=NULL) *u = ctx->uid;
	if(g!=NULL) *g = ctx->gid;
	if(p!=NULL) *p = ctx->pid;
	if(mask!=NULL) *mask = ctx->umask;
}

Fuse::~Fuse(){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
}

Fuse::VolumeInfo Fuse::findVolume(const boost::filesystem::path file_name){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    Fuse::VolumeInfo vinfo;

    Springy::Volumes::VolumeRelativeFile vols = this->config->volumes.getVolumesByVirtualFileName(file_name);
    vinfo.virtualMountPoint      = vols.virtualMountPoint;
    vinfo.volumeRelativeFileName = vols.volumeRelativeFileName;

    std::set<Springy::Volume::IVolume*>::iterator it;
    for(it=vols.volumes.begin();it != vols.volumes.end();it++){
        Springy::Volume::IVolume *volume = *it;
        if(volume->getattr(vols.volumeRelativeFileName, &vinfo.st) != -1){
            vinfo.volume   = volume;
            volume->statvfs(vols.volumeRelativeFileName, &vinfo.stvfs);
            //vinfo.curspace = vinfo.buf.f_bsize * vinfo.buf.f_bavail;

            return vinfo;
        }
    }

    throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "file not found";
}
Fuse::VolumeInfo Fuse::getMaxFreeSpaceVolume(const boost::filesystem::path path){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    Springy::Volumes::VolumeRelativeFile vols = this->config->volumes.getVolumesByVirtualFileName(path);
    
    Fuse::VolumeInfo vinfo;
    vinfo.virtualMountPoint      = vols.virtualMountPoint;
    vinfo.volumeRelativeFileName = vols.volumeRelativeFileName;

    Springy::Volume::IVolume* maxFreeSpaceVolume = NULL;
    struct statvfs stat;

    uintmax_t space = 0;

    std::set<Springy::Volume::IVolume*>::iterator it;
    for(it=vols.volumes.begin();it != vols.volumes.end();it++){
        Springy::Volume::IVolume *volume = *it;
        struct statvfs stvfs;

        volume->statvfs(vols.volumeRelativeFileName, &stvfs);
		uintmax_t curspace = stat.f_bsize * stat.f_bavail;

        if(maxFreeSpaceVolume == NULL || curspace > space){
            vinfo.volume = volume;
            vinfo.stvfs  = stvfs;
            volume->getattr(vols.volumeRelativeFileName, &vinfo.st);
            maxFreeSpaceVolume = volume;

            space = curspace;
        }
    }
    if(!maxFreeSpaceVolume){ return vinfo; }

    throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "no space";
}
boost::filesystem::path Fuse::get_parent_path(const boost::filesystem::path p){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    boost::filesystem::path parent = p.parent_path();
    if(parent.empty()){ throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "no parent directory"; }
    return parent;
}

boost::filesystem::path Fuse::get_remaining(const boost::filesystem::path p, const boost::filesystem::path front){
    boost::filesystem::path result;

    bool diff = false;

    boost::filesystem::path::iterator pit = p.begin(), fit = front.begin();
    for(;pit!=p.end() && fit!=front.end();pit++,fit++){
        if(*pit != *fit){
            diff = true;
            break;
        }
    }
    if(fit == front.end()){ diff = true; }
    if(diff){
        for(;pit!=p.end();pit++){
            result /= *pit;
        }
    }

    return result;
}

int Fuse::cloneParentDirsIntoVolume(Springy::Volume::IVolume *volume, const boost::filesystem::path path){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    boost::filesystem::path parent;
    try{
	    parent = this->get_parent_path(path);
    }catch(...){
        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return 0;
    }

	struct stat st;
	// already exists
    if (volume->getattr(parent, &st) == 0)
	{
        if(!S_ISDIR(st.st_mode)){
            errno = ENOTDIR;
            Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		    return -errno;
        }
		return 0;
	}

    Fuse::VolumeInfo vinfo;
    try{
        vinfo = Fuse::findVolume(parent);
    }
    catch(...){
        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -EFAULT;
    }

	// create parent dirs
	int res = this->cloneParentDirsIntoVolume(volume, parent);
	if (res!=0)
	{
		return res;
	}

	res = volume->mkdir(parent, st.st_mode);
	if (res==0)
	{
		volume->chown(parent, st.st_uid, st.st_gid);
		volume->chmod(parent, st.st_mode);
#ifndef WITHOUT_XATTR
        // copy extended attributes of parent dir
        this->copy_xattrs(vinfo.volume, volume, vinfo.volumeRelativeFileName);
#endif
	}
	else
	{
        //Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__) << volume->string() << " : " << parent.string();
        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__)  << (volume->string()) << " : " << parent.string();
		res=-errno;
	}

	return res;
}

int Fuse::copy_xattrs(Springy::Volume::IVolume *src, Springy::Volume::IVolume *dst, const boost::filesystem::path path){
#ifndef WITHOUT_XATTR
/*
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
*/
#endif
    return 0;
}
int Fuse::dir_is_empty(const boost::filesystem::path p){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	DIR * dir = this->libc->opendir(__LINE__, p.string().c_str());
	struct dirent *de;
	if (!dir){
        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -1;
    }
	while((de = this->libc->readdir(__LINE__, dir))) {
		if (this->libc->strcmp(__LINE__, de->d_name, ".") == 0) continue;
		if (this->libc->strcmp(__LINE__, de->d_name, "..") == 0) continue;
		this->libc->closedir(__LINE__, dir);
		return 0;
	}

	this->libc->closedir(__LINE__, dir);
	return 1;
}
void Fuse::reopen_files(const boost::filesystem::path file, const Springy::Volume::IVolume *volume){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
/*
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
*/
}

void Fuse::saveFd(boost::filesystem::path file, Springy::Volume::IVolume *volume, int fd, int flags){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    Synchronized syncOpenFiles(this->openFiles);

    int *syncToken = NULL;
    openFiles_set::index<of_idx_volumeFile>::type &idx = this->openFiles.get<of_idx_volumeFile>();
    openFiles_set::index<of_idx_volumeFile>::type::iterator it = idx.find(file);
    if(it!=idx.end()){
        syncToken = it->syncToken;
    }
    else{
        syncToken = new int;
    }

    struct openFile of = { file, volume, fd, flags, syncToken, true};
    this->openFiles.insert(of);
}

void Fuse::move_file(int fd, boost::filesystem::path file, Springy::Volume::IVolume *from, fsblkcnt_t wsize)
{
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
/*
	char *buf = NULL;
	ssize_t size;
	FILE *input = NULL, *output = NULL;
	struct utimbuf ftime = {0};
	fsblkcnt_t space;
	struct stat st;

    Springy::Volume::IVolume *to = this->getMaxFreeSpaceVolume(file, &space);
    if(space<wsize || from == to){
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

    this->create_parent_dirs(maxSpaceDir, file);

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
*/
}

void* Fuse::op_init(struct fuse_conn_info *conn){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    return static_cast<void*>(this);
}
void Fuse::op_destroy(void *arg){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	//mhddfs_httpd_stopServer();
}

/////////////////// File descriptor operations ////////////////////////////////

int Fuse::op_create(const boost::filesystem::path file, mode_t mode, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

    try{
        this->findVolume(file);
        // file exists
    }
    catch(...){
        Fuse::VolumeInfo vinfo;
        try{
             vinfo = this->getMaxFreeSpaceVolume(file);
        }
        catch(...){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            return -ENOSPC;
        }

        this->cloneParentDirsIntoVolume(vinfo.volume, vinfo.volumeRelativeFileName);

        // file doesnt exist
        int fd = vinfo.volume->creat(vinfo.volumeRelativeFileName, mode);
        if(fd == -1){
            return -errno;
        }
        try{
            this->saveFd(vinfo.volumeRelativeFileName, vinfo.volume, fd, O_CREAT|O_WRONLY|O_TRUNC);
            fi->fh = fd;
        }
        catch(...){
            if(errno==0){ errno = ENOMEM; }
            int rval = errno;
            vinfo.volume->close(vinfo.volumeRelativeFileName, fd);

            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            return -rval;
        }
        return 0;
    }

    return this->op_open(file, fi);
}

int Fuse::op_open(const boost::filesystem::path file, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(this->readonly && (fi->flags&O_RDONLY) != O_RDONLY){ return -EROFS; }

    fi->fh = 0;
    
    Fuse::VolumeInfo vinfo;

    try{
        int fd = 0;
        vinfo = this->findVolume(file);
        fd = vinfo.volume->open(vinfo.volumeRelativeFileName, fi->flags);
		if (fd == -1) {
			return -errno;
		}

        try{
            this->saveFd(vinfo.volumeRelativeFileName, vinfo.volume, fd, fi->flags);
        }
        catch(...){
            if(errno==0){ errno = ENOMEM; }
            int rval = errno;
            vinfo.volume->close(vinfo.volumeRelativeFileName, fd);

            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            return -rval;
        }

        fi->fh = fd;

		return 0;
    }
    catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }

    try{
         vinfo = this->getMaxFreeSpaceVolume(file);
    }
    catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -ENOSPC;
    }

	this->cloneParentDirsIntoVolume(vinfo.volume, vinfo.volumeRelativeFileName);

    int fd = -1;
	fd = vinfo.volume->open(vinfo.volumeRelativeFileName, fi->flags);

	if (fd == -1) {
		return -errno;
	}

	if (getuid() == 0) {
		struct stat st;
        gid_t gid;
        uid_t uid;
        this->determineCaller(&uid, &gid);
		if (vinfo.volume->getattr(vinfo.volumeRelativeFileName, &st) == 0) {
			// parent directory is SGID'ed
			if (st.st_gid != getgid()) gid = st.st_gid;
		}
		vinfo.volume->chown(vinfo.volumeRelativeFileName, uid, gid);
	}

    try{
        Synchronized syncOpenFiles(this->openFiles);

        int *syncToken = NULL;
        openFiles_set::index<of_idx_volumeFile>::type &idx = this->openFiles.get<of_idx_volumeFile>();
        openFiles_set::index<of_idx_volumeFile>::type::iterator it = idx.find(vinfo.volumeRelativeFileName);
        if(it!=idx.end()){
            syncToken = it->syncToken;
        }
        else{
            syncToken = new int;
        }

        struct openFile of = { vinfo.volumeRelativeFileName, vinfo.volume, fd, fi->flags, syncToken, true };
        this->openFiles.insert(of);
    }
    catch(...){
        if(errno==0){ errno = ENOMEM; }
        int rval = errno;
        vinfo.volume->close(vinfo.volumeRelativeFileName, fd);

        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        return -rval;
    }

    fi->fh = fd;

    return 0;
}

int Fuse::op_release(const boost::filesystem::path path, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    int fd = fi->fh;

    try{
        Synchronized syncOpenFiles(this->openFiles);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if(it==idx.end()){
            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "bad descriptor";
        }

        it->volume->close(path, fd);

        int *syncToken = it->syncToken;
        boost::filesystem::path volumeFile = it->volumeFile;
        idx.erase(it);

        openFiles_set::index<of_idx_volumeFile>::type &vidx = this->openFiles.get<of_idx_volumeFile>();
        if(vidx.find(volumeFile)==vidx.end()){
            delete syncToken;
        }
    }catch(...){
        if(errno==0){ errno = EBADFD; }

        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        return -errno;
    }

	return 0;
}

int Fuse::op_read(const boost::filesystem::path file, char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(buf==NULL){ return -EINVAL; }

    int fd = fi->fh;
    try{
        Synchronized syncOpenFiles(this->openFiles);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if(it!=idx.end() && it->valid==false){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "file descriptor has been invalidated internally");
            errno = EINVAL;
            return -errno;
        }

        Springy::Volume::IVolume *volume = it->volume;
        ssize_t res;
        res = volume->read(file, fd, buf, count, offset);
        if (res == -1){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        return res;
    }catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        errno = EBADFD;
        return -errno;
    }
}

int Fuse::op_write(const boost::filesystem::path file, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

    if(buf==NULL){ return -EINVAL; }

	ssize_t res;
    int fd = fi->fh;

    int *syncToken = NULL;
    Springy::Volume::IVolume *volume;

    try{
        Synchronized syncOpenFiles(this->openFiles);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if(it!=idx.end() && it->valid==false){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "file descriptor has been invalidated internally");
            errno = EINVAL;
            return -errno;
        }
        if(it==idx.end()){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "file descriptor not found");
            errno = EBADFD;
            return -errno;
        }

        syncToken = it->syncToken;
        volume = it->volume;
    }catch(...){
        if(errno==0){ errno = EBADFD; }
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "exception catched");
        return -errno;
    }

    Synchronized sync(syncToken);

    errno = 0;
	res = volume->write(file, fd, buf, count, offset);
	if ((res >= 0) || (res == -1 && errno != ENOSPC)) {
		if (res == -1) {
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
			return -errno;
		}
		return res;
	}

    struct stat st;
    volume->getattr(file, &st);

    // write failed, cause of no space left
    errno = ENOSPC;
    try{
        this->move_file(fd, file, volume, (off_t)(offset+count) > st.st_size ? offset+count : st.st_size);
    }catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "exception catched");
        return -errno;
    }

    res = this->libc->pwrite(__LINE__, fd, buf, count, offset);
    if (res == -1) {
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "write failed");
        return -errno;
    }

    return res;
}

int Fuse::op_ftruncate(const boost::filesystem::path path, off_t size, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

    int fd = fi->fh;
    try{
        Synchronized syncOpenFiles(this->openFiles);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if(it!=idx.end() && it->valid==false){
            // MISSING: log that the descriptor has been invalidated as the reason of failing
            errno = EINVAL;
            return -errno;
        }
        if(it->volume->truncate(it->volumeFile, fd, size) == -1){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		    return -errno;
        }
        return 0;
    }catch(...){}

    errno = -EBADFD;
    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    return -errno;
}

int Fuse::op_fsync(const boost::filesystem::path path, int isdatasync, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

    int fd = fi->fh;
    try{
        Synchronized syncOpenFiles(this->openFiles);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if(it!=idx.end() && it->valid==false){
            errno = EINVAL;
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        int res = 0;
        res = it->volume->fsync(path, fd);

        if (res == -1)
            return -errno;
    }catch(...){
        errno = EBADFD;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -errno;
    }

	return 0;
}

int Fuse::op_lock(const boost::filesystem::path path, struct fuse_file_info *fi, int cmd, struct flock *lck){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    int fd = fi->fh;
    try{
        Synchronized syncOpenFiles(this->openFiles);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if(it!=idx.end() && it->valid==false){
            errno = EINVAL;
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        int res = it->volume->lock(path, fd, cmd, lck, &fi->lock_owner);
        if (res == -1)
            return -errno;
    }catch(...){
        errno = EBADFD;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -errno;
    }

	return 0;
}

/////////////////// Path based operations ////////////////////////////////

int Fuse::op_truncate(const boost::filesystem::path path, off_t size){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

    try{
        Fuse::VolumeInfo vinfo = this->findVolume(path);
        int res = vinfo.volume->truncate(vinfo.volumeRelativeFileName, -1, size);

		if (res == -1){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
			return -errno;
        }
		return 0;
    }catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }
    errno = ENOENT;
    return -errno;
}

int Fuse::op_getattr(const boost::filesystem::path file_name, struct stat *buf){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(buf==NULL){ return -EINVAL; }

	try{
		VolumeInfo vinfo = this->findVolume(file_name);
        *buf = vinfo.st;
		return 0;
	}
	catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }

    return -ENOENT;
}

int Fuse::op_statfs(const boost::filesystem::path path, struct statvfs *buf){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(buf==NULL){ return -EINVAL; }

    std::vector<struct statvfs> stats;
    std::set<dev_t> localDevices;

    try{
		this->findVolume(path);
	}
	catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -ENOENT;
    }

    unsigned long min_block = 0, min_frame = 0;

    Springy::Volumes::VolumeRelativeFile vols = this->config->volumes.getVolumesByVirtualFileName(path);

    std::set<Springy::Volume::IVolume*>::iterator it;
    for(it=vols.volumes.begin();it != vols.volumes.end();it++){
        Springy::Volume::IVolume *volume = *it;
        if(volume->isLocal()){
            struct stat st;
            int ret = volume->getattr(vols.volumeRelativeFileName, &st);
            if (ret != 0) {
                return -errno;
            }
            if(localDevices.find(st.st_dev) == localDevices.end()){ continue; }
        }

        struct statvfs stv;
        int ret = volume->statvfs(vols.volumeRelativeFileName, &stv);
        if (ret != 0) {
            return -errno;
        }

        stats.push_back(stv);

		if(stats.size()==1){
			min_block = stv.f_bsize,
			min_frame = stv.f_frsize;
		}
		else{
			if (min_block>stv.f_bsize) min_block = stv.f_bsize;
			if (min_frame>stv.f_frsize) min_frame = stv.f_frsize;
		}
	}

	if (!min_block)
		min_block = 512;
	if (!min_frame)
		min_frame = 512;

    for (unsigned int i=0;i < stats.size();i++) {
		if (stats[i].f_bsize>min_block) {
			stats[i].f_bfree    *=  stats[i].f_bsize/min_block;
			stats[i].f_bavail   *=  stats[i].f_bsize/min_block;
			stats[i].f_bsize    =   min_block;
		}
		if (stats[i].f_frsize>min_frame) {
			stats[i].f_blocks   *=  stats[i].f_frsize/min_frame;
			stats[i].f_frsize   =   min_frame;
		}

		if(i == 0){
			memcpy(buf, &stats[i], sizeof(struct statvfs));
			continue;
		}

		if (buf->f_namemax < stats[i].f_namemax) {
			buf->f_namemax = stats[i].f_namemax;
		}
		buf->f_ffree  +=  stats[i].f_ffree;
		buf->f_files  +=  stats[i].f_files;
		buf->f_favail +=  stats[i].f_favail;
		buf->f_bavail +=  stats[i].f_bavail;
		buf->f_bfree  +=  stats[i].f_bfree;
		buf->f_blocks +=  stats[i].f_blocks;
	}

	return 0;
}

int Fuse::op_readdir(
		const boost::filesystem::path dirname,
		void *buf,
		fuse_fill_dir_t filler,
		off_t offset,
		struct fuse_file_info * fi)
{
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    std::unordered_map<std::string, struct stat> directories;
    std::unordered_map<std::string, struct stat>::iterator dit;
    struct stat st;
	size_t found=0;
    std::vector<Springy::Volume::IVolume*> dirs;

    Springy::Volumes::VolumeRelativeFile vols = this->config->volumes.getVolumesByVirtualFileName(dirname);

    std::set<Springy::Volume::IVolume*>::iterator it;
    for(it=vols.volumes.begin();it != vols.volumes.end();it++){
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
	for (size_t i = 0; i<dirs.size(); i++) {
        Springy::Volume::IVolume *volume = dirs[i];
        volume->readdir(vols.volumeRelativeFileName, directories);
	}

    for(dit=directories.begin();dit!=directories.end();dit++){
        if (filler(buf, dit->first.c_str(), &dit->second, 0))
                break;
    }

	return 0;
}

int Fuse::op_readlink(const boost::filesystem::path path, char *buf, size_t size){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(buf==NULL){ return -EINVAL; }

    int res = 0;
    try{
        Fuse::VolumeInfo vinfo = this->findVolume(path);
        memset(buf, 0, size);
        res = vinfo.volume->readlink(vinfo.volumeRelativeFileName, buf, size);

        if (res >= 0)
            return 0;
        return -errno;
    }
    catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }
	return -ENOENT;
}

int Fuse::op_access(const boost::filesystem::path path, int mode){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(this->readonly && (mode&F_OK) != F_OK && (mode&R_OK) != R_OK){ return -EROFS; }

    try{
        Fuse::VolumeInfo vinfo = this->findVolume(path);
        int res = vinfo.volume->access(vinfo.volumeRelativeFileName, mode);

		if (res == -1){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
			return -errno;
        }
		return 0;
    }catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }
    errno = ENOENT;
    return -errno;
}

int Fuse::op_mkdir(const boost::filesystem::path path, mode_t mode){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

    Fuse::VolumeInfo vinfo;
    try{
        vinfo = this->findVolume(path);

        errno = EEXIST;
        return -errno;
    }catch(...){}

    try{
        boost::filesystem::path parent = this->get_parent_path(path);
        this->findVolume(parent);
    }
    catch(...){
        errno = ENOENT;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -errno;
    }

    try{
        vinfo = this->getMaxFreeSpaceVolume(path);
    }
    catch(...){
		errno = ENOSPC;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -errno;
    }

    this->cloneParentDirsIntoVolume(vinfo.volume, vinfo.volumeRelativeFileName);
	if (vinfo.volume->mkdir(vinfo.volumeRelativeFileName, mode) == 0) {
		if (this->libc->getuid(__LINE__) == 0) {
			struct stat st;
            gid_t gid;
            uid_t uid;
            this->determineCaller(&uid, &gid);
			if (vinfo.volume->getattr(vinfo.volumeRelativeFileName, &st) == 0) {
				// parent directory is SGID'ed
				if (st.st_gid != this->libc->getgid(__LINE__))
					gid = st.st_gid;
			}
			vinfo.volume->chown(vinfo.volumeRelativeFileName, uid, gid);
		}
		return 0;
	}

    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
	return -errno;
}
int Fuse::op_rmdir(const boost::filesystem::path path){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

    try{
        Fuse::VolumeInfo vinfo = this->findVolume(path);
        int res = vinfo.volume->rmdir(vinfo.volumeRelativeFileName);
        if (res == -1){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        return 0;
    }catch(...){}

    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    return -ENOENT;
}

int Fuse::op_unlink(const boost::filesystem::path path){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

    try{
        Fuse::VolumeInfo vinfo = this->findVolume(path);
        int res = vinfo.volume->unlink(vinfo.volumeRelativeFileName);

        if (res == -1){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }

        return 0;
    }catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		errno = ENOENT;
		return -errno;
    }
}

int Fuse::op_rename(const boost::filesystem::path from, const boost::filesystem::path to){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

    int res;
    struct stat st;

	if (from == to)
		return 0;

    boost::filesystem::path fromParent = this->get_parent_path(from);
    boost::filesystem::path toParent = this->get_parent_path(to);

    Springy::Volumes::VolumeRelativeFile fromVolumes = this->config->volumes.getVolumesByVirtualFileName(from);

    std::set<Springy::Volume::IVolume*>::iterator it;
    for(it=fromVolumes.volumes.begin();it != fromVolumes.volumes.end();it++){
        if((*it)->getattr(fromVolumes.volumeRelativeFileName, &st)!=0){ continue; }

        res = (*it)->rename(fromVolumes.volumeRelativeFileName, to);
        if (res == -1) {
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }
    }

	return 0;
}

int Fuse::op_utimens(const boost::filesystem::path path, const struct timespec ts[2]){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

	size_t flag_found = 0;
    int res;
    struct stat st;

    Springy::Volumes::VolumeRelativeFile pathVolumes = this->config->volumes.getVolumesByVirtualFileName(path);

    std::set<Springy::Volume::IVolume*>::iterator it;
    for(it=pathVolumes.volumes.begin();it != pathVolumes.volumes.end();it++){
        if((*it)->getattr(pathVolumes.volumeRelativeFileName, &st)!=0){ continue; }
        
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

int Fuse::op_chmod(const boost::filesystem::path path, mode_t mode){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

	size_t flag_found;
    int res;
    struct stat st;
    
    Springy::Volumes::VolumeRelativeFile pathVolumes = this->config->volumes.getVolumesByVirtualFileName(path);

    std::set<Springy::Volume::IVolume*>::iterator it;
    for(it=pathVolumes.volumes.begin();it != pathVolumes.volumes.end();it++){
        if((*it)->getattr(pathVolumes.volumeRelativeFileName, &st)!=0){ continue; }

        flag_found = 1;
        res = (*it)->chmod(pathVolumes.volumeRelativeFileName, mode);

		if (res == -1){
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

int Fuse::op_chown(const boost::filesystem::path path, uid_t uid, gid_t gid){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

	size_t flag_found;
    int res;
    struct stat st;

    Springy::Volumes::VolumeRelativeFile pathVolumes = this->config->volumes.getVolumesByVirtualFileName(path);

    std::set<Springy::Volume::IVolume*>::iterator it;
    for(it=pathVolumes.volumes.begin();it != pathVolumes.volumes.end();it++){
        if((*it)->getattr(pathVolumes.volumeRelativeFileName, &st)!=0){ continue; }

        flag_found = 1;
        res = (*it)->chown(pathVolumes.volumeRelativeFileName, uid, gid);

		if (res == -1){
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

int Fuse::op_symlink(const boost::filesystem::path oldname, const boost::filesystem::path newname){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(this->readonly){ return -EROFS; }

	int res;
    
    // symlink only works on same volume type
    Fuse::VolumeInfo vinfo;
    boost::filesystem::path parent;
    try{
        parent = this->get_parent_path(newname);
        vinfo = this->findVolume(parent);
    }catch(...){
        errno = ENOENT;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -errno;
    }

    // symlink into found Volume
    res = vinfo.volume->symlink(this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, oldname),
                                this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, newname));
    if(res==0){
        return 0;
    }
    if(errno != ENOSPC){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -errno;
    }

    try{
        vinfo = this->getMaxFreeSpaceVolume(parent);
    }catch(...){
        errno = ENOSPC;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -errno;
    }

    // symlink into max free space volume
    this->cloneParentDirsIntoVolume(vinfo.volume, this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, newname));
    res = vinfo.volume->symlink(this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, oldname),
                                this->config->volumes.convertFuseFilenameToVolumeRelativeFilename(vinfo.volume, newname));
    if(res==0){
        return 0;
    }
    if(errno != ENOSPC){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -errno;
    }

    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
	return -errno;
}

int Fuse::op_link(const boost::filesystem::path oldname, const boost::filesystem::path newname){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

    int res = 0;
    Fuse::VolumeInfo vinfo;

    try{
        vinfo = this->findVolume(oldname);
    }catch(...){
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

int Fuse::op_mknod(const boost::filesystem::path path, mode_t mode, dev_t rdev){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    
    if(this->readonly){ return -EROFS; }

	int res, i;
    boost::filesystem::path parent;
    Fuse::VolumeInfo vinfo;
    try{
        parent = this->get_parent_path(path);
        vinfo = this->findVolume(parent);
    }catch(...){
        errno = ENOENT;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -errno;
    }

	for (i = 0; i < 2; i++) {
		if (i) {
            try{
                vinfo = this->getMaxFreeSpaceVolume(parent);
            }catch(...){
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
                uid_t uid;
                gid_t gid;
                this->determineCaller(&uid, &gid);
				vinfo.volume->chown(path, uid, gid);
			}

			return 0;
		}

		if (errno != ENOSPC){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
			return -errno;
        }
	}
    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
	return -errno;
}


//#ifndef WITHOUT_XATTR
//int Fuse::op_setxattr(const boost::filesystem::path file_name, const std::string attrname,
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

//int Fuse::op_getxattr(const boost::filesystem::path file_name, const std::string attrname, char *buf, size_t count){
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

//int Fuse::op_listxattr(const boost::filesystem::path file_name, char *buf, size_t count){
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

//int Fuse::op_removexattr(const boost::filesystem::path file_name, const std::string attrname){
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




////// static functions to forward function call to Fuse* instance

void* Fuse::init(struct fuse_conn_info *conn){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_init(conn);
}
void Fuse::destroy(void *arg){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_destroy(arg);
}

int Fuse::getattr(const char *path, struct stat *buf){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_getattr(boost::filesystem::path(path), buf);
}
int Fuse::statfs(const char *path, struct statvfs *buf){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_statfs(path, buf);
}

int Fuse::readdir(const char *dirname, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info * fi){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_readdir(boost::filesystem::path(dirname), buf, filler, offset, fi);
}
int Fuse::readlink(const char *path, char *buf, size_t size){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_readlink(boost::filesystem::path(path), buf, size);
}

int Fuse::create(const char *path, mode_t mode, struct fuse_file_info *fi){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_create(boost::filesystem::path(path), mode, fi);
}
int Fuse::open(const char *path, struct fuse_file_info *fi){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_open(boost::filesystem::path(path), fi);
}
int Fuse::release(const char *path, struct fuse_file_info *fi){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_open(boost::filesystem::path(path), fi);
}
int Fuse::read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_read(path, buf, count, offset, fi);
}
int Fuse::write(const char *path, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_write(boost::filesystem::path(path), buf, count, offset, fi);
}
int Fuse::truncate(const char *path, off_t size){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_truncate(boost::filesystem::path(path), size);
}
int Fuse::ftruncate(const char *path, off_t size, struct fuse_file_info *fi){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_ftruncate(boost::filesystem::path(path), size, fi);
}
int Fuse::access(const char *path, int mask){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_access(boost::filesystem::path(path), mask);
}
int Fuse::mkdir(const char *path, mode_t mode){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_mkdir(boost::filesystem::path(path), mode);
}
int Fuse::rmdir(const char *path){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_rmdir(path);
}
int Fuse::unlink(const char *path){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_unlink(path);
}
int Fuse::rename(const char *from, const char *to){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_rename(boost::filesystem::path(from), boost::filesystem::path(to));
}
int Fuse::utimens(const char *path, const struct timespec ts[2]){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_utimens(boost::filesystem::path(path), ts);
}
int Fuse::chmod(const char *path, mode_t mode){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_chmod(boost::filesystem::path(path), mode);
}
int Fuse::chown(const char *path, uid_t uid, gid_t gid){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_chown(boost::filesystem::path(path), uid, gid);
}
int Fuse::symlink(const char *from, const char *to){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_symlink(boost::filesystem::path(from), boost::filesystem::path(to));
}
int Fuse::link(const char *from, const char *to){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_link(boost::filesystem::path(from), boost::filesystem::path(to));
}
int Fuse::mknod(const char *path, mode_t mode, dev_t rdev){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_mknod(boost::filesystem::path(path), mode, rdev);
}
int Fuse::fsync(const char *path, int isdatasync, struct fuse_file_info *fi){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_fsync(boost::filesystem::path(path), isdatasync, fi);
}

int Fuse::lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lck){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_lock(boost::filesystem::path(path), fi, cmd, lck);
}

int Fuse::setxattr(const char *path, const char *attrname,
					const char *attrval, size_t attrvalsize, int flags){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    //struct fuse_context *ctx = fuse_get_context();
    //Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    //return instance->op_setxattr(boost::filesystem::path(path), attrname, attrval, attrvalsize, flags);
    return -ENOTSUP;
}
int Fuse::getxattr(const char *path, const char *attrname, char *buf, size_t count){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    //struct fuse_context *ctx = fuse_get_context();
    //Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    //return instance->op_getxattr(boost::filesystem::path(path), attrname, buf, count);
    return -ENOTSUP;
}
int Fuse::listxattr(const char *path, char *buf, size_t count){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    //struct fuse_context *ctx = fuse_get_context();
    //Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    //return instance->op_listxattr(boost::filesystem::path(path), buf, count);
    return -ENOTSUP;
}
int Fuse::removexattr(const char *path, const char *attrname){
    std::cout << __FILE__ << ":" << __LINE__ << ":" << __PRETTY_FUNCTION__ << std::endl;
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	//struct fuse_context *ctx = fuse_get_context();
    //Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    //return instance->op_removexattr(boost::filesystem::path(path), attrname);
    return -ENOTSUP;
}


}

#endif
