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
	    fuse_exit(this->fuse);
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

int Fuse::countDirectoryElements(boost::filesystem::path p){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	int depth = 0;

	while(p.has_relative_path()){
        if(p.string() == "/"){ break; }
		depth++;
		p = p.parent_path();
	}

	return depth;
}

int Fuse::countEquals(const boost::filesystem::path &p1, const boost::filesystem::path &p2){
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

boost::filesystem::path Fuse::concatPath(const boost::filesystem::path &p1, const boost::filesystem::path &p2){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    boost::filesystem::path p = boost::filesystem::path(p1/p2);
    while(p.string().back() == '/'){
        p = p.parent_path();
    }

    return p;
}

Springy::Volume::IVolume* Fuse::findVolume(boost::filesystem::path file_name, struct stat *buf){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	struct stat b;
	if(buf==NULL){ buf = &b; }
    boost::filesystem::path foundPath;

    std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > vols = this->config->volumes.getVolumesByVirtualFileName(file_name);

    std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> >::iterator it;
    for(it=vols.begin();it != vols.end();it++){
        if(it->first->lstat(file_name, buf) != -1){
            return it->first;
        }
    }

    throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "file not found";
}
Springy::Volume::IVolume* Fuse::getMaxFreeSpaceVolume(boost::filesystem::path path, uintmax_t *space){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> > vols = this->config->volumes.getVolumesByVirtualFileName(path);

    Springy::Volume::IVolume* maxFreeSpaceVolume = NULL;
    struct statvfs stat;

    uintmax_t max_space = 0;
    if(space==NULL){
        space = &max_space;
    }
    *space = 0;

    std::set<std::pair<Springy::Volume::IVolume*, boost::filesystem::path> >::iterator it;
    for(it=vols.begin();it != vols.end();it++){
        it->first->statvfs(path, &stat);
		uintmax_t curspace = stat.f_bsize * stat.f_bavail;

        if(maxFreeSpaceVolume == NULL || curspace > *space){
            maxFreeSpaceVolume = it->first;
            *space = curspace;
        }
    }
    if(!maxFreeSpaceVolume){ return maxFreeSpaceVolume; }

    throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "no space";
}
boost::filesystem::path Fuse::get_parent_path(const boost::filesystem::path p){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    boost::filesystem::path parent = p.parent_path();
    if(parent.empty()){ throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "no parent directory"; }
    return parent;
}

boost::filesystem::path Fuse::get_base_name(const boost::filesystem::path p){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    return p.filename();
}

int Fuse::create_parent_dirs(boost::filesystem::path dir, const boost::filesystem::path path){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    boost::filesystem::path parent;
    try{
	    parent = this->get_parent_path(path);
    }catch(...){
        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return 0;
    }

	boost::filesystem::path exists;
    try{
        exists = this->findPath(parent);
    }
    catch(...){
        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -EFAULT;
    }

	boost::filesystem::path path_parent = this->concatPath(dir, parent);
	struct stat st;

	// already exists
	if (this->libc->stat(__LINE__, path_parent.c_str(), &st)==0)
	{
        if(!S_ISDIR(st.st_mode)){
            errno = ENOTDIR;
            Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		    return -errno;
        }
		return 0;
	}

	// create parent dirs
	int res = this->create_parent_dirs(dir, parent);
	if (res!=0)
	{
		return res;
	}

	// get stat from exists dir
	if (this->libc->stat(__LINE__, exists.c_str(), &st)!=0)
	{
        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -errno;
	}
	res=this->libc->mkdir(__LINE__, path_parent.c_str(), st.st_mode);
	if (res==0)
	{
		this->libc->chown(__LINE__, path_parent.c_str(), st.st_uid, st.st_gid);
		this->libc->chmod(__LINE__, path_parent.c_str(), st.st_mode);
#ifndef WITHOUT_XATTR
        // copy extended attributes of parent dir
        this->copy_xattrs(exists, path_parent);
#endif
	}
	else
	{
        Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__) << path_parent.string();
		res=-errno;
	}

	return res;
}
#ifndef WITHOUT_XATTR
int Fuse::copy_xattrs(const boost::filesystem::path from, const boost::filesystem::path to){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    int listsize=0, attrvalsize=0;
    char *listbuf=NULL, *attrvalbuf=NULL,
            *name_begin=NULL, *name_end=NULL;

    // if not xattrs on source, then do nothing
    if ((listsize=this->libc->listxattr(__LINE__, from.c_str(), NULL, 0)) == 0)
            return 0;

    // get all extended attributes
    listbuf=(char *)this->libc->calloc(__LINE__, sizeof(char), listsize);
    if (this->libc->listxattr(__LINE__, from.c_str(), listbuf, listsize) == -1)
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
        attrvalsize = this->libc->getxattr(__LINE__, from.c_str(), name_begin, NULL, 0);
        if (attrvalsize < 0)
        {
            Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -1;
        }

        // get the value of the extended attribute
        attrvalbuf=(char *)this->libc->calloc(__LINE__, sizeof(char), attrvalsize);
        if (this->libc->getxattr(__LINE__, from.c_str(), name_begin, attrvalbuf, attrvalsize) < 0)
        {
            Trace(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -1;
        }

        // set the value of the extended attribute on dest file
        if (this->libc->setxattr(__LINE__, to.c_str(), name_begin, attrvalbuf, attrvalsize, 0) < 0)
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
    return 0;
}
#endif
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
void Fuse::reopen_files(const boost::filesystem::path file, const boost::filesystem::path newDirectory){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    boost::filesystem::path newFile = this->concatPath(newDirectory, file);
    Synchronized syncOpenFiles(this->openFiles);

    openFiles_set::index<of_idx_fuseFile>::type &idx = this->openFiles.get<of_idx_fuseFile>();
    std::pair<openFiles_set::index<of_idx_fuseFile>::type::iterator,
              openFiles_set::index<of_idx_fuseFile>::type::iterator> range = idx.equal_range(file);

    openFiles_set::index<of_idx_fuseFile>::type::iterator it;
    for(it = range.first;it!=range.second;it++){

        off_t seek = lseek(it->fd, 0, SEEK_CUR);
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

void Fuse::saveFd(boost::filesystem::path file, boost::filesystem::path usedPath, int fd, int flags){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    Synchronized syncOpenFiles(this->openFiles);

    int *syncToken = NULL;
    openFiles_set::index<of_idx_fuseFile>::type &idx = this->openFiles.get<of_idx_fuseFile>();
    openFiles_set::index<of_idx_fuseFile>::type::iterator it = idx.find(file);
    if(it!=idx.end()){
        syncToken = it->syncToken;
    }
    else{
        syncToken = new int;
    }

    struct openFile of = { file, usedPath/file, fd, flags, syncToken, true};
    this->openFiles.insert(of);
}

void Fuse::move_file(int fd, boost::filesystem::path file, boost::filesystem::path from, fsblkcnt_t wsize)
{
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	char *buf = NULL;
	ssize_t size;
	FILE *input = NULL, *output = NULL;
	struct utimbuf ftime = {0};
	fsblkcnt_t space;
	struct stat st;

    boost::filesystem::path maxSpaceDir = this->getMaxFreeSpaceDir(file, &space);
    if(space<wsize || from.parent_path() == maxSpaceDir){
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "not enough space";
    }

	/* get file size */
	if (this->libc->fstat(__LINE__, fd, &st) != 0) {
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "fstat failed";
	}


    /* Hard link support is limited to a single device, and files with
       >1 hardlinks cannot be moved between devices since this would
       (a) result in partial files on the source device (b) not free
       the space from the source device during unlink. */
	if (st.st_nlink > 1) {
        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "cannot move file with hard links";
	}

    this->create_parent_dirs(maxSpaceDir, file);

    //boost::filesystem::path from = this->concatPath(directory, file);
    boost::filesystem::path to = this->concatPath(maxSpaceDir, file);

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
        this->copy_xattrs(from, to);
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

void* Fuse::op_init(struct fuse_conn_info *conn){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    return static_cast<void*>(this);
}
void Fuse::op_destroy(void *arg){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	//mhddfs_httpd_stopServer();
}

int Fuse::op_getattr(const boost::filesystem::path file_name, struct stat *buf){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(buf==NULL){ return -EINVAL; }

	try{
		this->findPath(file_name, buf);
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

	std::map<dev_t, struct statvfs> stats;
	std::map<dev_t, struct statvfs>::iterator it;

    try{
		this->findPath(path);
	}
	catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -ENOENT;
    }

    std::map<boost::filesystem::path, boost::filesystem::path> directories = this->findRelevantPaths(path);

    unsigned long min_block = 0, min_frame = 0;

    std::map<boost::filesystem::path, boost::filesystem::path>::iterator dit;
	for(dit=directories.begin();dit != directories.end();dit++){
		struct stat st;
		int ret = stat(dit->first.c_str(), &st);
		if (ret != 0) {
			return -errno;
		}
		if(stats.find(st.st_dev)!=stats.end()){ continue; }

		struct statvfs stv;
		ret = this->libc->statvfs(__LINE__, dit->first.c_str(), &stv);
		if (ret != 0) {
			return -errno;
		}

		stats[st.st_dev] = stv;

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

    for (it = stats.begin(); it != stats.end(); it++) {
		if (it->second.f_bsize>min_block) {
			it->second.f_bfree    *=  it->second.f_bsize/min_block;
			it->second.f_bavail   *=  it->second.f_bsize/min_block;
			it->second.f_bsize    =   min_block;
		}
		if (it->second.f_frsize>min_frame) {
			it->second.f_blocks   *=  it->second.f_frsize/min_frame;
			it->second.f_frsize   =   min_frame;
		}

		if(it==stats.begin()){
			memcpy(buf, &it->second, sizeof(struct statvfs));
			continue;
		}

		if (buf->f_namemax<it->second.f_namemax) {
			buf->f_namemax = it->second.f_namemax;
		}
		buf->f_ffree  +=  it->second.f_ffree;
		buf->f_files  +=  it->second.f_files;
		buf->f_favail +=  it->second.f_favail;
		buf->f_bavail +=  it->second.f_bavail;
		buf->f_bfree  +=  it->second.f_bfree;
		buf->f_blocks +=  it->second.f_blocks;
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

    std::unordered_map<std::string, struct stat> dir_item_ht;
    std::unordered_map<std::string, struct stat>::iterator it;
    struct stat st;
	size_t found=0;
    std::vector<boost::filesystem::path> dirs;

	std::map<boost::filesystem::path, boost::filesystem::path> directories = this->findRelevantPaths(dirname);

	// find all dirs
    std::map<boost::filesystem::path, boost::filesystem::path>::iterator dit;
	for(dit=directories.begin();dit != directories.end();dit++){

		boost::filesystem::path path = this->concatPath(dit->first, dirname);
		if (this->libc->stat(__LINE__, path.c_str(), &st) == 0) {
			found++;
			if (S_ISDIR(st.st_mode)) {
                dirs.push_back(path);
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
		struct dirent *de;
		DIR * dh = this->libc->opendir(__LINE__, dirs[i].c_str());
		if (!dh){
			continue;
        }

		while((de = this->libc->readdir(__LINE__, dh))) {
			// find dups
            if(dir_item_ht.find(de->d_name)!=dir_item_ht.end()){
                continue;
            }

			// add item
            boost::filesystem::path object_name = this->concatPath(dirs[i], de->d_name);

            this->libc->lstat(__LINE__, object_name.c_str(), &st);
            dir_item_ht.insert(std::make_pair(de->d_name, st));
		}

		this->libc->closedir(__LINE__, dh);
	}

    for(it=dir_item_ht.begin();it!=dir_item_ht.end();it++){
        if (filler(buf, it->first.c_str(), &it->second, 0))
                break;
    }

	return 0;
}

int Fuse::op_readlink(const boost::filesystem::path path, char *buf, size_t size){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(buf==NULL){ return -EINVAL; }

    int res = 0;
    try{
        boost::filesystem::path link = this->findPath(path);
        memset(buf, 0, size);
        res = this->libc->readlink(__LINE__, link.c_str(), buf, size);

        if (res >= 0)
            return 0;
        return -errno;
    }
    catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }
	return -ENOENT;
}

int Fuse::op_create(const boost::filesystem::path file, mode_t mode, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    try{
        this->findPath(file);
        // file exists
    }
    catch(...){
        boost::filesystem::path maxFreeDir;
        try{
             maxFreeDir = this->getMaxFreeSpaceDir(file);
        }
        catch(...){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            return -ENOSPC;
        }

        this->create_parent_dirs(maxFreeDir, file);
        boost::filesystem::path path = this->concatPath(maxFreeDir, file);

        // file doesnt exist
        int fd = this->libc->creat(__LINE__, path.c_str(), mode);
        if(fd == -1){
            return -errno;
        }
        try{
            this->saveFd(file, maxFreeDir, fd, O_CREAT|O_WRONLY|O_TRUNC);
            fi->fh = fd;
        }
        catch(...){
            if(errno==0){ errno = ENOMEM; }
            int rval = errno;
            this->libc->close(__LINE__, fd);

            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            return -rval;
        }
        return 0;
    }

    return this->op_open(file, fi);
}

int Fuse::op_open(const boost::filesystem::path file, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    fi->fh = 0;

    try{
        int fd = 0;
        boost::filesystem::path usedPath;
	    boost::filesystem::path path = this->findPath(file, NULL, &usedPath);
		fd = this->libc->open(__LINE__, path.c_str(), fi->flags);
		if (fd == -1) {
			return -errno;
		}

        try{
            this->saveFd(file, usedPath, fd, fi->flags);
        }
        catch(...){
            if(errno==0){ errno = ENOMEM; }
            int rval = errno;
            this->libc->close(__LINE__, fd);

            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);

            return -rval;
        }

        fi->fh = fd;

		return 0;
    }
    catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }

    boost::filesystem::path maxFreeDir;
    try{
         maxFreeDir = this->getMaxFreeSpaceDir(file);
    }
    catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -ENOSPC;
    }

	this->create_parent_dirs(maxFreeDir, file);
	boost::filesystem::path path = this->concatPath(maxFreeDir, file);

    int fd = -1;
	fd = this->libc->open(__LINE__, path.c_str(), fi->flags);

	if (fd == -1) {
		return -errno;
	}

	if (getuid() == 0) {
		struct stat st;
        gid_t gid;
        uid_t uid;
        this->determineCaller(&uid, &gid);
		if (fstat(fd, &st) == 0) {
			// parent directory is SGID'ed
			if (st.st_gid != getgid()) gid = st.st_gid;
		}
		fchown(fd, uid, gid);
	}

    try{
        Synchronized syncOpenFiles(this->openFiles);

        int *syncToken = NULL;
        openFiles_set::index<of_idx_fuseFile>::type &idx = this->openFiles.get<of_idx_fuseFile>();
        openFiles_set::index<of_idx_fuseFile>::type::iterator it = idx.find(path);
        if(it!=idx.end()){
            syncToken = it->syncToken;
        }
        else{
            syncToken = new int;
        }

        struct openFile of = { file, maxFreeDir, fd, fi->flags, syncToken, true};
        this->openFiles.insert(of);
    }
    catch(...){
        if(errno==0){ errno = ENOMEM; }
        int rval = errno;
        this->libc->close(__LINE__, fd);

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
        this->libc->close(__LINE__, fd);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if(it==idx.end()){
            throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "bad descriptor";
        }
        int *syncToken = it->syncToken;
        boost::filesystem::path fuseFile = it->fuseFile;
        idx.erase(it);

        openFiles_set::index<of_idx_fuseFile>::type &fidx = this->openFiles.get<of_idx_fuseFile>();
        if(fidx.find(fuseFile)==fidx.end()){
            delete syncToken;
        }
    }catch(...){
        if(errno==0){ errno = EBADFD; }

        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);

        return -errno;
    }

	return 0;
}

int Fuse::op_read(const boost::filesystem::path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
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
    }catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }

	ssize_t res;
	res = this->libc->pread(__LINE__, fd, buf, count, offset);

	if (res == -1){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -errno;
    }
	return res;
}

int Fuse::op_write(const boost::filesystem::path file, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    if(buf==NULL){ return -EINVAL; }

	ssize_t res;
    int fd = fi->fh;

    int *syncToken = NULL;
    boost::filesystem::path path;

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
        path = it->path;
    }catch(...){
        if(errno==0){ errno = EBADFD; }
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__, "exception catched");
        return -errno;
    }

    Synchronized sync(syncToken);

    errno = 0;
	res = this->libc->pwrite(__LINE__, fd, buf, count, offset);
	if ((res >= 0) || (res == -1 && errno != ENOSPC)) {
		if (res == -1) {
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
			return -errno;
		}
		return res;
	}

    struct stat st;
    fstat(fd, &st);

    // write failed, cause of no space left
    errno = ENOSPC;
    try{
        this->move_file(fd, file, path, (off_t)(offset+count) > st.st_size ? offset+count : st.st_size);
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

int Fuse::op_truncate(const boost::filesystem::path path, off_t size){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    try{
        boost::filesystem::path file = this->findPath(path);
        int res = this->libc->truncate(__LINE__, file.c_str(), size);

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
int Fuse::op_ftruncate(const boost::filesystem::path path, off_t size, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

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
    }catch(...){}

	if (this->libc->ftruncate(__LINE__, fd, size) == -1){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -errno;
    }
	return 0;
}

int Fuse::op_access(const boost::filesystem::path path, int mask){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    try{
        boost::filesystem::path file = this->findPath(path);
        int res = this->libc->access(__LINE__, file.c_str(), mask);

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

    try{
        this->findPath(path);
        errno = EEXIST;
        return -errno;
    }catch(...){}

	boost::filesystem::path parent;
    try{
        parent = this->get_parent_path(path);
        this->findPath(parent);
    }
    catch(...){
        errno = EFAULT;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -errno;
    }

    boost::filesystem::path dir;
    try{
        dir = this->getMaxFreeSpaceDir(path);
    }
    catch(...){
		errno = ENOSPC;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -errno;
    }

    this->create_parent_dirs(dir, path);
	boost::filesystem::path name = this->concatPath(dir, path);
	if (this->libc->mkdir(__LINE__, name.c_str(), mode) == 0) {
		if (getuid() == 0) {
			struct stat st;
            gid_t gid;
            uid_t uid;
            this->determineCaller(&uid, &gid);
			if (this->libc->lstat(__LINE__, name.c_str(), &st) == 0) {
				// parent directory is SGID'ed
				if (st.st_gid != getgid())
					gid = st.st_gid;
			}
			this->libc->chown(__LINE__, name.c_str(), uid, gid);
		}
		return 0;
	}

    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
	return -errno;
}
int Fuse::op_rmdir(const boost::filesystem::path path){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    boost::filesystem::path dir;
    int res = 0;

    try{
        dir = this->findPath(path);
        res = this->libc->rmdir(__LINE__, dir.c_str());
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

    int res = 0;

    try{
	    boost::filesystem::path file = this->findPath(path);
        res = this->libc->unlink(__LINE__, file.c_str());
    }catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		errno = ENOENT;
		return -errno;
    }

	if (res == -1){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -errno;
    }
	return 0;
}

int Fuse::op_rename(const boost::filesystem::path from, const boost::filesystem::path to){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    int res;
	struct stat sto, sfrom;
	int from_is_dir = 0, to_is_dir = 0, from_is_file = 0, to_is_file = 0;
	int to_dir_is_empty = 1;

	if (from == to)
		return 0;

    boost::filesystem::path fromParent = this->get_parent_path(from);
    boost::filesystem::path toParent = this->get_parent_path(to);

	Synchronized sDirs(this->config->directories, Synchronized::LockType::READ);

	// seek for possible errors
    std::map<boost::filesystem::path, boost::filesystem::path>::iterator dit;
	for(dit=this->config->directories.begin();dit != this->config->directories.end();dit++){
        // if virtual mountpoint
        if(dit->second.string().find(fromParent.string())!=0 || dit->second.string().find(toParent.string())!=0){ continue; }

		boost::filesystem::path obj_to   = this->concatPath(dit->first, to);
		boost::filesystem::path obj_from = this->concatPath(dit->first, from);
		if (this->libc->stat(__LINE__, obj_to.c_str(), &sto) == 0) {
			if (S_ISDIR(sto.st_mode)) {
				to_is_dir++;
				if (!this->dir_is_empty(obj_to))
					to_dir_is_empty = 0;
			}
			else
				to_is_file++;
		}
		if (this->libc->stat(__LINE__, obj_from.c_str(), &sfrom) == 0) {
			if (S_ISDIR (sfrom.st_mode))
				from_is_dir++;
			else
				from_is_file++;
		}

        errno = 0;
		if (to_is_file && from_is_dir){
            errno = ENOTDIR;
        }
		else if (to_is_file && to_is_dir){
            errno = ENOTEMPTY;
        }
		else if (from_is_dir && !to_dir_is_empty){
            errno = ENOTEMPTY;
        }
        if(errno != 0){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
			return -errno;
        }
	}

	// parent 'to' path doesn't exists
	boost::filesystem::path pto = this->get_parent_path(to);
    try{
        this->findPath(pto);
    }
    catch(...){
        errno = ENOENT;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -errno;
    }

	// rename cycle
	for(dit=this->config->directories.begin();dit != this->config->directories.end();dit++){
        if(dit->second.string().find(fromParent.string())!=0 || dit->second.string().find(toParent.string())!=0){ continue; }

		boost::filesystem::path obj_to   = this->concatPath(dit->first, to);
		boost::filesystem::path obj_from = this->concatPath(dit->first, from);

		if (this->libc->stat(__LINE__, obj_from.c_str(), &sfrom) == 0) {
			// if from is dir and at the same time file, we only rename dir
			if (from_is_dir && from_is_file) {
				if (!S_ISDIR(sfrom.st_mode)) {
					continue;
				}
			}

			this->create_parent_dirs(dit->first, to);

			res = this->libc->rename(__LINE__, obj_from.c_str(), obj_to.c_str());
			if (res == -1) {
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
				return -errno;
			}
		} else {
			// from and to are files, so we must remove to files
			if (from_is_file && to_is_file && !from_is_dir) {
				if (this->libc->stat(__LINE__, obj_to.c_str(), &sto) == 0) {
					if (this->libc->unlink(__LINE__, obj_to.c_str()) == -1) {
                        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
						return -errno;
					}
				}
			}
		}

	}

	return 0;
}

int Fuse::op_utimens(const boost::filesystem::path path, const struct timespec ts[2]){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	size_t flag_found;
    int res;
    struct stat st;

	Synchronized sDirs(this->config->directories, Synchronized::LockType::READ);

    Springy::Settings::DirectoryMap::const_iterator dit = this->config->directories.begin();
	for(flag_found=0;dit != this->config->directories.end();dit++){
		boost::filesystem::path object = this->concatPath(dit->first, path);
		if (this->libc->lstat(__LINE__, object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;

        res = this->libc->utimensat(__LINE__, AT_FDCWD, object.c_str(), ts, AT_SYMLINK_NOFOLLOW);

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

int Fuse::op_chmod(const boost::filesystem::path path, mode_t mode){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	size_t flag_found;
    int res;
    struct stat st;

	Synchronized sDirs(this->config->directories, Synchronized::LockType::READ);

    Springy::Settings::DirectoryMap::const_iterator dit = this->config->directories.begin();
	for(flag_found=0;dit != this->config->directories.end();dit++){
		boost::filesystem::path object = this->concatPath(dit->first, path);
		if (this->libc->lstat(__LINE__, object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;
		res = this->libc->chmod(__LINE__, object.c_str(), mode);

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

	size_t flag_found;
    int res;
    struct stat st;

	Synchronized sDirs(this->config->directories, Synchronized::LockType::READ);

    Springy::Settings::DirectoryMap::const_iterator dit = this->config->directories.begin();
	for(flag_found=0;dit != this->config->directories.end();dit++){
		boost::filesystem::path object = this->concatPath(dit->first, path);
		if (this->libc->lstat(__LINE__, object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;
		res = this->libc->lchown(__LINE__, object.c_str(), uid, gid);
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

	int i, res;

    boost::filesystem::path parent, directory;
    try{
        parent = this->get_parent_path(newname);
        directory = this->findPath(parent);
    }catch(...){
        errno = ENOENT;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -errno;
    }

	for (i = 0; i < 2; i++) {
		if (i) {
            try{
			    directory = this->getMaxFreeSpaceDir(parent);
            }catch(...){
				errno = ENOSPC;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
				return -errno;
			}

			this->create_parent_dirs(directory, newname);
		}

		boost::filesystem::path path_to = this->concatPath(directory, newname);

		res = this->libc->symlink(__LINE__, oldname.c_str(), path_to.c_str());

		if (res == 0)
			return 0;
		if (errno != ENOSPC){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
			return -errno;
        }
	}
    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
	return -errno;
}

int Fuse::op_link(const boost::filesystem::path oldname, const boost::filesystem::path newname){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    int res = 0;
    boost::filesystem::path usedPath;

    try{
        this->findPath(oldname, NULL, &usedPath);
    }catch(...){
		errno = ENOENT;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -errno;
    }

	res = this->create_parent_dirs(usedPath, newname);
	if (res != 0) {
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return res;
	}
    

	boost::filesystem::path path_oldname = this->concatPath(usedPath, oldname);
	boost::filesystem::path path_newname = this->concatPath(usedPath, newname);

	res = this->libc->link(__LINE__, path_oldname.c_str(), path_newname.c_str());

	if (res == 0)
		return 0;
    t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
	return -errno;
}

int Fuse::op_mknod(const boost::filesystem::path path, mode_t mode, dev_t rdev){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	int res, i;
    boost::filesystem::path parent, directory;
    try{
        parent = this->get_parent_path(path);
        directory = this->findPath(parent);
    }catch(...){
        errno = ENOENT;
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
		return -errno;
    }

	for (i = 0; i < 2; i++) {
		if (i) {
            try{
                directory = this->getMaxFreeSpaceDir(parent);
            }catch(...){
				errno = ENOSPC;
                t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
				return -errno;
            }

			this->create_parent_dirs(directory, path);
		}
		boost::filesystem::path nod = this->concatPath(directory, path);

		if (S_ISREG(mode)) {
			res = this->libc->open(__LINE__, nod.c_str(), O_CREAT | O_EXCL | O_WRONLY, mode);
			if (res >= 0)
				res = close(res);
		} else if (S_ISFIFO(mode))
			res = this->libc->mkfifo(__LINE__, nod.c_str(), mode);
		else
			res = this->libc->mknod(__LINE__, nod.c_str(), mode, rdev);

		if (res != -1) {
			if (this->libc->getuid(__LINE__) == 0) {
                uid_t uid;
                gid_t gid;
                this->determineCaller(&uid, &gid);
				this->libc->chown(__LINE__, nod.c_str(), uid, gid);
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

#if _POSIX_SYNCHRONIZED_IO + 0 > 0 || defined(__FreeBSD__)
#undef HAVE_FDATASYNC
#else
#define HAVE_FDATASYNC 1
#endif

int Fuse::op_fsync(const boost::filesystem::path path, int isdatasync, struct fuse_file_info *fi){
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
    }catch(...){}

    int res = 0;

#ifdef HAVE_FDATASYNC
	if (isdatasync)
		res = this->libc->fdatasync(__LINE__, fd);
	else
#endif
		res = this->libc->fsync(__LINE__, fd);

	if (res == -1)
		return -errno;
	return 0;
}

#ifndef WITHOUT_XATTR
int Fuse::op_setxattr(const boost::filesystem::path file_name, const std::string attrname,
                const char *attrval, size_t attrvalsize, int flags){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	try{
		boost::filesystem::path path = this->findPath(file_name);
        if (this->libc->setxattr(__LINE__, path.c_str(), attrname.c_str(), attrval, attrvalsize, flags) == -1){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
         }
		return 0;
	}
	catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }
	return -ENOENT;
}

int Fuse::op_getxattr(const boost::filesystem::path file_name, const std::string attrname, char *buf, size_t count){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	try{
		boost::filesystem::path path = this->findPath(file_name);
        int size = this->libc->getxattr(__LINE__, path.c_str(), attrname.c_str(), buf, count);
        if(size == -1){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }
		return size;
	}
	catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }
	return -ENOENT;
}

int Fuse::op_listxattr(const boost::filesystem::path file_name, char *buf, size_t count){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    try{
		boost::filesystem::path path = this->findPath(file_name);
		int ret = 0;
        if((ret=this->libc->listxattr(__LINE__, path.c_str(), buf, count)) == -1){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }
		return ret;
	}
	catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }
	return -ENOENT;
}

int Fuse::op_removexattr(const boost::filesystem::path file_name, const std::string attrname){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	try{
		boost::filesystem::path path = this->findPath(file_name);
        if(this->libc->removexattr(__LINE__, path.c_str(), attrname.c_str()) == -1){
            t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
            return -errno;
        }
		return 0;
	}
	catch(...){
        t.log(__FILE__, __PRETTY_FUNCTION__, __LINE__);
    }
	return -ENOENT;
}
#endif




////// static functions to forward function call to Fuse* instance

void* Fuse::init(struct fuse_conn_info *conn){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->init(conn);
}
void Fuse::destroy(void *arg){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->destroy(arg);
}

int Fuse::getattr(const char *path, struct stat *buf){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_getattr(boost::filesystem::path(path), buf);
}
int Fuse::statfs(const char *path, struct statvfs *buf){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_statfs(path, buf);
}

int Fuse::readdir(const char *dirname, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info * fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_readdir(boost::filesystem::path(dirname), buf, filler, offset, fi);
}
int Fuse::readlink(const char *path, char *buf, size_t size){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_readlink(boost::filesystem::path(path), buf, size);
}

int Fuse::create(const char *path, mode_t mode, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_create(boost::filesystem::path(path), mode, fi);
}
int Fuse::open(const char *path, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_open(boost::filesystem::path(path), fi);
}
int Fuse::release(const char *path, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_open(boost::filesystem::path(path), fi);
}
int Fuse::read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_read(path, buf, count, offset, fi);
}
int Fuse::write(const char *path, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_write(boost::filesystem::path(path), buf, count, offset, fi);
}
int Fuse::truncate(const char *path, off_t size){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_truncate(boost::filesystem::path(path), size);
}
int Fuse::ftruncate(const char *path, off_t size, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_ftruncate(boost::filesystem::path(path), size, fi);
}
int Fuse::access(const char *path, int mask){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_access(boost::filesystem::path(path), mask);
}
int Fuse::mkdir(const char *path, mode_t mode){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_mkdir(boost::filesystem::path(path), mode);
}
int Fuse::rmdir(const char *path){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_rmdir(path);
}
int Fuse::unlink(const char *path){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_unlink(path);
}
int Fuse::rename(const char *from, const char *to){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_rename(boost::filesystem::path(from), boost::filesystem::path(to));
}
int Fuse::utimens(const char *path, const struct timespec ts[2]){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_utimens(boost::filesystem::path(path), ts);
}
int Fuse::chmod(const char *path, mode_t mode){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_chmod(boost::filesystem::path(path), mode);
}
int Fuse::chown(const char *path, uid_t uid, gid_t gid){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_chown(boost::filesystem::path(path), uid, gid);
}
int Fuse::symlink(const char *from, const char *to){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_symlink(boost::filesystem::path(from), boost::filesystem::path(to));
}
int Fuse::link(const char *from, const char *to){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_link(boost::filesystem::path(from), boost::filesystem::path(to));
}
int Fuse::mknod(const char *path, mode_t mode, dev_t rdev){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_mknod(boost::filesystem::path(path), mode, rdev);
}
int Fuse::fsync(const char *path, int isdatasync, struct fuse_file_info *fi){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_fsync(boost::filesystem::path(path), isdatasync, fi);
}

int Fuse::setxattr(const char *path, const char *attrname,
					const char *attrval, size_t attrvalsize, int flags){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_setxattr(boost::filesystem::path(path), attrname, attrval, attrvalsize, flags);
}
int Fuse::getxattr(const char *path, const char *attrname, char *buf, size_t count){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_getxattr(boost::filesystem::path(path), attrname, buf, count);
}
int Fuse::listxattr(const char *path, char *buf, size_t count){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_listxattr(boost::filesystem::path(path), buf, count);
}
int Fuse::removexattr(const char *path, const char *attrname){
    Trace t(__FILE__, __PRETTY_FUNCTION__, __LINE__);

	struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_removexattr(boost::filesystem::path(path), attrname);
}


}

#endif
