#ifdef HAS_FUSE

#include "fuse.hpp"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <dirent.h>

#include "util/synchronized.hpp"

#include <set>
#include <map>
#include <sstream>
#include <boost/algorithm/string/join.hpp>
#include <boost/exception/diagnostic_information.hpp> 

namespace Springy{

const char *Fuse::fsname = "springy";

Fuse::Fuse(){
    this->fuse   = NULL;
    this->config = NULL;
    this->libc   = NULL;
}
Fuse& Fuse::init(Settings *config, Springy::LibC::ILibC *libc){
    if(config == NULL || libc == NULL){
        throw std::runtime_error("invalid argument given");
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
    this->mountpoint = this->config->option<std::string>("mountpoint");

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
    
    std::set<std::string> options = this->config->option<std::set<std::string> >("options");
    this->fuseoptions = boost::algorithm::join(options, ",");
    if(this->fuseoptions.size()>0){
        fuseArgv.push_back("-o");
        fuseArgv.push_back(this->fuseoptions.c_str());
    }

    this->fuse = fuse_setup(fuseArgv.size(), (char**)&fuseArgv[0], &this->fops, sizeof(this->fops),
                      &this->fmountpoint, &multithreaded, static_cast<void*>(this));

    if(this->fuse==NULL){
        // fuse failed!
        throw std::runtime_error("initializing fuse failed");
    }

    return *this;
}
Fuse& Fuse::run(){
    this->th = std::thread(Fuse::thread, this);

    return *this;
}
Fuse& Fuse::tearDown(){
    if(this->fuse && !fuse_exited(this->fuse)){
        fuse_teardown(this->fuse, this->fmountpoint);
	    fuse_exit(this->fuse);
        this->fmountpoint = NULL;
	}

    if(this->th.joinable()){
        struct stat buf;
        this->libc->stat(this->mountpoint.c_str(), &buf);

        // wait for 
        try{
            this->th.join();
        }catch(...){}
    }

    return *this;
}

void Fuse::thread(Fuse *instance){
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
	struct fuse_context *ctx = fuse_get_context();
	if(ctx==NULL) return;
	if(u!=NULL) *u = ctx->uid;
	if(g!=NULL) *g = ctx->gid;
	if(p!=NULL) *p = ctx->pid;
	if(mask!=NULL) *mask = ctx->umask;
}

Fuse::~Fuse(){}

std::string Fuse::concatPath(const std::string &p1, const std::string &p2){
    std::string rval = p1;
    if(rval[rval.size()-1]!='/'){ rval += "/"; }
    if(p2[0]=='/'){
        rval += p2.substr(1);
    }
    else{
        rval += p2;
    }

    return rval;
}
std::string Fuse::findPath(std::string file_name, struct stat *buf, std::string *usedPath){
	struct stat b;
	if(buf==NULL){ buf = &b; }

	std::map<std::string, std::string> &directories = this->config->option<std::map<std::string, std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    std::map<std::string, std::string>::iterator it;
	for(it=directories.begin();it != directories.end();it++){
        // MISSING: handle virtual mountpoint
		std::string path = this->concatPath(it->first, file_name);

		if(this->libc->lstat(path.c_str(), buf) != -1){
            if(usedPath!=NULL){
                usedPath->assign(it->first);
            }
            return path;
        }
    }
    throw std::runtime_error("file not found");
}
std::string Fuse::getMaxFreeSpaceDir(fsblkcnt_t *space){
	std::map<std::string, std::string> &directories = this->config->option<std::map<std::string, std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    std::string maxFreeSpaceDir;
    fsblkcnt_t max_space = 0;
    if(space==NULL){
        space = &max_space;
    }
    *space = 0;

    std::map<std::string, std::string>::iterator it;
	for(it=directories.begin();it != directories.end();it++){
        struct statvfs stf;
		if (statvfs(it->first.c_str(), &stf) != 0)
			continue;
		fsblkcnt_t curspace  = stf.f_bsize;
		curspace *= stf.f_bavail;
        
        if(curspace>*space){
            maxFreeSpaceDir = it->first;
            *space = curspace;
        }
    }
    if(maxFreeSpaceDir.size()>0){ return maxFreeSpaceDir; }

    throw std::runtime_error("no space");
}
std::string Fuse::get_parent_path(const std::string path)
{
	std::string dir = path;
	int len=dir.size();
	if (len && dir[len-1]=='/'){ --len; dir.pop_back(); }
	while(len && dir[len-1]!='/'){ --len; dir.pop_back(); }
	if (len>1 && dir[len-1]=='/'){ --len; dir.pop_back(); }
	if (len) return dir;

	throw std::runtime_error("no parent directory");
}

std::string Fuse::get_base_name(const std::string path)
{
	std::string dir = path;
	int len = dir.size();
	if (len && dir[len-1]=='/'){ --len; dir.pop_back(); }
    std::string::size_type filePos = dir.rfind('/');
	if (filePos != std::string::npos)
	{
        dir = dir.substr(filePos+1);
	}
	return dir;
}

int Fuse::create_parent_dirs(std::string dir, const std::string path)
{
    std::string parent;
    try{
	    parent = this->get_parent_path(path);
    }catch(...){
        return 0;
    }

	std::string exists;
    try{
        exists = this->findPath(parent);
    }
    catch(...){
        return -EFAULT;
    }

	std::string path_parent = this->concatPath(dir, parent);
	struct stat st;

	// already exists
	if (this->libc->stat(path_parent.c_str(), &st)==0)
	{
		return 0;
	}

	// create parent dirs
	int res = this->create_parent_dirs(dir, parent);

	if (res!=0)
	{
		return res;
	}

	// get stat from exists dir
	if (this->libc->stat(exists.c_str(), &st)!=0)
	{
		return -errno;
	}
	res=this->libc->mkdir(path_parent.c_str(), st.st_mode);
	if (res==0)
	{
		this->libc->chown(path_parent.c_str(), st.st_uid, st.st_gid);
		this->libc->chmod(path_parent.c_str(), st.st_mode);
	}
	else
	{
		res=-errno;
	}

#ifndef WITHOUT_XATTR
        // copy extended attributes of parent dir
        this->copy_xattrs(exists, path_parent);
#endif

	return res;
}
#ifndef WITHOUT_XATTR
int Fuse::copy_xattrs(const std::string from, const std::string to)
{
        int listsize=0, attrvalsize=0;
        char *listbuf=NULL, *attrvalbuf=NULL,
                *name_begin=NULL, *name_end=NULL;

        // if not xattrs on source, then do nothing
        if ((listsize=this->libc->listxattr(from.c_str(), NULL, 0)) == 0)
                return 0;

        // get all extended attributes
        listbuf=(char *)calloc(sizeof(char), listsize);
        if (this->libc->listxattr(from.c_str(), listbuf, listsize) == -1)
        {
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
                attrvalsize = this->libc->getxattr(from.c_str(), name_begin, NULL, 0);
                if (attrvalsize < 0)
                {
                        return -1;
                }

                // get the value of the extended attribute
                attrvalbuf=(char *)calloc(sizeof(char), attrvalsize);
                if (this->libc->getxattr(from.c_str(), name_begin, attrvalbuf, attrvalsize) < 0)
                {
                        return -1;
                }

                // set the value of the extended attribute on dest file
                if (this->libc->setxattr(to.c_str(), name_begin, attrvalbuf, attrvalsize, 0) < 0)
                {
                        return -1;
                }

                this->libc->free(attrvalbuf);

                // point the pointer to the start of the attr name to the start
                // of the next attr
                name_begin=name_end+1;
                name_end++;
        }

        this->libc->free(listbuf);
        return 0;
}
#endif
int Fuse::dir_is_empty(const std::string path)
{
	DIR * dir = opendir(path.c_str());
	struct dirent *de;
	if (!dir)
		return -1;
	while((de = this->libc->readdir(dir))) {
		if (this->libc->strcmp(de->d_name, ".") == 0) continue;
		if (this->libc->strcmp(de->d_name, "..") == 0) continue;
		this->libc->closedir(dir);
		return 0;
	}

	this->libc->closedir(dir);
	return 1;
}
void Fuse::reopen_files(const std::string file, const std::string newDirectory)
{
    std::string newFile = this->concatPath(newDirectory, file);
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
        if ((fh = this->libc->open(newFile.c_str(), flags)) == -1) {
            it->valid = false;
            continue;
        }
        else
        {
            // seek
            if (seek != this->libc->lseek(fh, seek, SEEK_SET)) {
                this->libc->close(fh);
                it->valid = false;
                continue;
            }

            // filehandle
            if (this->libc->dup2(fh, it->fd) != it->fd) {
                this->libc->close(fh);
                it->valid = false;
                continue;
            }
            // close temporary filehandle
            this->libc->close(fh);
        }

        range.first->path = newDirectory;
    }
}

void Fuse::saveFd(std::string file, std::string usedPath, int fd, int flags){
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

    struct openFile of = { file, usedPath, fd, flags, syncToken, true};
    this->openFiles.insert(of);
}

void Fuse::move_file(int fd, std::string file, std::string directory, fsblkcnt_t wsize)
{
	char *buf = NULL;
	ssize_t size;
	FILE *input = NULL, *output = NULL;
	struct utimbuf ftime = {0};
	fsblkcnt_t space;
	struct stat st;

    std::string maxSpaceDir = this->getMaxFreeSpaceDir(&space);
    if(space<wsize || directory == maxSpaceDir){
        throw std::runtime_error("not enough space");
    }


	/* get file size */
	if (fstat(fd, &st) != 0) {
        throw std::runtime_error("fstat failed");
	}


    /* Hard link support is limited to a single device, and files with
       >1 hardlinks cannot be moved between devices since this would
       (a) result in partial files on the source device (b) not free
       the space from the source device during unlink. */
	if (st.st_nlink > 1) {
        errno = ENOTSUP;
        throw std::runtime_error("cannot move file with hard links");
	}

    this->create_parent_dirs(maxSpaceDir, file);

    std::string from = this->concatPath(directory, file);
    std::string to = this->concatPath(maxSpaceDir, file);

    // in case fd is not open for reading - just open a new file pointer
	if (!(input = fopen(from.c_str(), "r")))
		throw std::runtime_error("open file for reading failed");

	if (!(output = fopen(to.c_str(), "w+"))) {
        this->libc->fclose(input);
        throw std::runtime_error("open file for writing failed");
	}
    
    int inputfd = fileno(input);
    int outputfd = fileno(output);

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
        this->libc->fclose(input);
        this->libc->fclose(output);
        this->libc->unlink(to.c_str());
        throw std::runtime_error("not enough memory");
    }

	while((size = this->libc->read(inputfd, buf, sizeof(char)*allocationSize))>0) {
        ssize_t written = 0;
        while(written<size){
            size_t bytesWritten = this->libc->write(outputfd, buf+written, sizeof(char)*(size-written));
            if(bytesWritten>0){
                written += bytesWritten;
            }
            else{
                this->libc->fclose(input);
                this->libc->fclose(output);
                delete[] buf;
                this->libc->unlink(to.c_str());
                throw std::runtime_error("moving file failed");
            }
        }
	}

    delete[] buf;
	if(size==-1){
        this->libc->fclose(input);
        this->libc->fclose(output);
        this->libc->unlink(to.c_str());
        throw std::runtime_error("read error occured");
    }

	this->libc->fclose(input);

	// owner/group/permissions
	this->libc->fchmod(outputfd, st.st_mode);
	this->libc->fchown(outputfd, st.st_uid, st.st_gid);
	this->libc->fclose(output);

	// time
	ftime.actime = st.st_atime;
	ftime.modtime = st.st_mtime;
	this->libc->utime(to.c_str(), &ftime);

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
        this->libc->unlink(to.c_str());
        throw std::runtime_error("failed to modify internal data structure");
    }

    try{
        this->reopen_files(file, maxSpaceDir);
        this->libc->unlink(from.c_str());
    }catch(...){
        this->libc->unlink(to.c_str());
        throw std::runtime_error("failed to reopen already open files");
    }
}

void* Fuse::op_init(struct fuse_conn_info *conn){
    return static_cast<void*>(this);
}
void Fuse::op_destroy(void *arg){
	//mhddfs_httpd_stopServer();
}

int Fuse::op_getattr(const std::string file_name, struct stat *buf)
{
    if(buf==NULL){ return -EINVAL; }

	try{
		std::string path = this->findPath(file_name, buf);
		return 0;
	}
	catch(...){}

    return -ENOENT;
}

int Fuse::op_statfs(const std::string path, struct statvfs *buf)
{
    if(buf==NULL){ return -EINVAL; }

	std::map<dev_t, struct statvfs> stats;
	std::map<dev_t, struct statvfs>::iterator it;

    try{
		this->findPath(path);
	}
	catch(...){
        return -ENOENT;
    }

    std::map<std::string, std::string> &directories = this->config->option<std::map<std::string, std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    unsigned long min_block = 0, min_frame = 0;

    std::map<std::string, std::string>::iterator dit;
	for(dit=directories.begin();dit != directories.end();dit++){
        std::string vmountpoint = dit->second;
        if(path.find(vmountpoint)==std::string::npos){
            continue;
        }

		struct stat st;
		int ret = stat(dit->first.c_str(), &st);
		if (ret != 0) {
			return -errno;
		}
		if(stats.find(st.st_dev)!=stats.end()){ continue; }

		struct statvfs stv;
		ret = this->libc->statvfs(dit->first.c_str(), &stv);
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
		const std::string dirname,
		void *buf,
		fuse_fill_dir_t filler,
		off_t offset,
		struct fuse_file_info * fi)
{
    std::unordered_map<std::string, struct stat> dir_item_ht;
    std::unordered_map<std::string, struct stat>::iterator it;
    struct stat st;
	size_t found=0;
    std::vector<std::string> dirs;

	std::map<std::string, std::string> &directories = this->config->option<std::map<std::string, std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

	// find all dirs
    std::map<std::string, std::string>::iterator dit;
	for(dit=directories.begin();dit != directories.end();dit++){
        if(dirname.find(dit->second)==std::string::npos){
            continue;
        }

		std::string path = this->concatPath(dit->first, dirname);
		if (this->libc->stat(path.c_str(), &st) == 0) {
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
		DIR * dh = this->libc->opendir(dirs[i].c_str());
		if (!dh){
			continue;
        }

		while((de = this->libc->readdir(dh))) {
			// find dups
            if(dir_item_ht.find(de->d_name)!=dir_item_ht.end()){
                continue;
            }

			// add item
            std::string object_name = this->concatPath(dirs[i], de->d_name);

            this->libc->lstat(object_name.c_str(), &st);
            dir_item_ht.insert(std::make_pair(de->d_name, st));
		}

		this->libc->closedir(dh);
	}

    for(it=dir_item_ht.begin();it!=dir_item_ht.end();it++){
        if (filler(buf, it->first.c_str(), &it->second, 0))
                break;
    }

	return 0;
}

int Fuse::op_readlink(const std::string path, char *buf, size_t size)
{
    if(buf==NULL){ return -EINVAL; }

    int res = 0;
    try{
        std::string link = this->findPath(path);
        memset(buf, 0, size);
        res = this->libc->readlink(link.c_str(), buf, size);

        if (res >= 0)
            return 0;
        return -errno;
    }
    catch(...){}
	return -ENOENT;
}

int Fuse::op_create(const std::string file, mode_t mode, struct fuse_file_info *fi)
{
    try{
        this->findPath(file);
        // file exists
    }
    catch(...){
        std::string maxFreeDir;
        try{
             maxFreeDir = this->getMaxFreeSpaceDir();
        }
        catch(...){
            return -ENOSPC;
        }

        this->create_parent_dirs(maxFreeDir, file);
        std::string path = this->concatPath(maxFreeDir, file);

        // file doesnt exist
        int fd = this->libc->creat(path.c_str(), mode);
        if(fd == -1){
            return -errno;
        }
        try{
            this->saveFd(file, maxFreeDir, fd, O_CREAT|O_WRONLY|O_TRUNC);
            fi->fh = fd;
        }
        catch(...){
            // MISSING: log instead of cout std::cout << __FILE__ << ":" << __LINE__ << ":" << boost::current_exception_diagnostic_information() << std::endl;
            if(errno==0){ errno = ENOMEM; }
            int rval = errno;
            this->libc->close(fd);
            return -rval;
        }
        return 0;

    }

    return this->op_open(file, fi);
}

int Fuse::op_open(const std::string file, struct fuse_file_info *fi)
{
    fi->fh = 0;

    try{
        int fd = 0;
        std::string usedPath;
	    std::string path = this->findPath(file, NULL, &usedPath);
		fd = this->libc->open(path.c_str(), fi->flags);
		if (fd == -1) {
			return -errno;
		}

        try{
            this->saveFd(file, usedPath, fd, fi->flags);
        }
        catch(...){
            // MISSING: log instead of cout std::cout << __FILE__ << ":" << __LINE__ << ":" << boost::current_exception_diagnostic_information() << std::endl;
            if(errno==0){ errno = ENOMEM; }
            int rval = errno;
            this->libc->close(fd);
            return -rval;
        }

        fi->fh = fd;

		return 0;
    }
    catch(...){
        // MISSING: log instead of cout std::cout << __FILE__ << ":" << __LINE__ << ":" << boost::current_exception_diagnostic_information() << std::endl;
    }

    std::string maxFreeDir;
    try{
         maxFreeDir = this->getMaxFreeSpaceDir();
    }
    catch(...){
        return -ENOSPC;
    }

	this->create_parent_dirs(maxFreeDir, file);
	std::string path = this->concatPath(maxFreeDir, file);

    int fd = -1;
	fd = this->libc->open(path.c_str(), fi->flags);

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
        // MISSING: log instead of cout std::cout << __FILE__ << ":" << __LINE__ << ":" << boost::current_exception_diagnostic_information() << std::endl;
        if(errno==0){ errno = ENOMEM; }
        int rval = errno;
        this->libc->close(fd);
        return -rval;
    }

    fi->fh = fd;

	return 0;
}

int Fuse::op_release(const std::string path, struct fuse_file_info *fi)
{
    int fd = fi->fh;

    try{
        Synchronized syncOpenFiles(this->openFiles);
        this->libc->close(fd);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if(it==idx.end()){
            throw std::runtime_error("bad descriptor");
        }
        int *syncToken = it->syncToken;
        std::string fuseFile = it->fuseFile;
        idx.erase(it);

        openFiles_set::index<of_idx_fuseFile>::type &fidx = this->openFiles.get<of_idx_fuseFile>();
        if(fidx.find(fuseFile)==fidx.end()){
            delete syncToken;
        }
    }catch(...){
        // MISSING: log instead of cout std::cout << __FILE__ << ":" << __LINE__ << ":" << boost::current_exception_diagnostic_information() << std::endl;
        if(errno==0){ errno = EBADFD; }
        return -errno;
    }

	return 0;
}

int Fuse::op_read(const std::string, char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
    if(buf==NULL){ return -EINVAL; }

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

	ssize_t res;
	res = this->libc->pread(fd, buf, count, offset);

	if (res == -1)
		return -errno;
	return res;
}

int Fuse::op_write(const std::string file, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
    if(buf==NULL){ return -EINVAL; }

	ssize_t res;
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
    }catch(...){
        // MISSING: log instead of cout std::cout << __FILE__ << ":" << __LINE__ << ":" << boost::current_exception_diagnostic_information() << std::endl;
    }

    int *syncToken = NULL;
    std::string path;

    try{
        Synchronized syncOpenFiles(this->openFiles);

        openFiles_set::index<of_idx_fd>::type &idx = this->openFiles.get<of_idx_fd>();
        openFiles_set::index<of_idx_fd>::type::iterator it = idx.find(fd);
        if(it==idx.end()){
            errno = EBADFD;
            return -errno;
        }
        syncToken = it->syncToken;
        path = it->path;
    }catch(...){
        // MISSING: log instead of cout std::cout << __FILE__ << ":" << __LINE__ << ":" << boost::current_exception_diagnostic_information() << std::endl;
        if(errno==0){ errno = EBADFD; }
        return -errno;
    }

    Synchronized sync(syncToken);

    errno = 0;
	res = this->libc->pwrite(fd, buf, count, offset);
	if ((res >= 0) || (res == -1 && errno != ENOSPC)) {
		if (res == -1) {
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
        // MISSING: log instead of cout std::cout << __FILE__ << ":" << __LINE__ << ":" << boost::current_exception_diagnostic_information() << std::endl;
        return -errno;
    }

    res = this->libc->pwrite(fd, buf, count, offset);
    if (res == -1) {
        return -errno;
    }

    return res;
}

int Fuse::op_truncate(const std::string path, off_t size)
{
    try{
        std::string file = this->findPath(path);
        int res = this->libc->truncate(file.c_str(), size);

		if (res == -1)
			return -errno;
		return 0;
    }catch(...){}
    errno = ENOENT;
    return -errno;
}
int Fuse::op_ftruncate(const std::string path, off_t size, struct fuse_file_info *fi)
{
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

	if (this->libc->ftruncate(fd, size) == -1)
		return -errno;
	return 0;
}

int Fuse::op_access(const std::string path, int mask)
{
    try{
        std::string file = this->findPath(path);
        int res = this->libc->access(file.c_str(), mask);

		if (res == -1)
			return -errno;
		return 0;
    }catch(...){}
    errno = ENOENT;
    return -errno;
}

int Fuse::op_mkdir(const std::string path, mode_t mode)
{
    try{
        this->findPath(path);
        errno = EEXIST;
        return -errno;
    }catch(...){}

	std::string parent;
    try{
        parent = this->get_parent_path(path);
        this->findPath(parent);
    }
    catch(...){
        errno = EFAULT;
        return -errno;
    }

    std::string dir;
    try{
        dir = this->getMaxFreeSpaceDir();
    }
    catch(...){
		errno = ENOSPC;
		return -errno;
    }

    this->create_parent_dirs(dir, path);
	std::string name = this->concatPath(dir, path);
	if (this->libc->mkdir(name.c_str(), mode) == 0) {
		if (getuid() == 0) {
			struct stat st;
            gid_t gid;
            uid_t uid;
            this->determineCaller(&uid, &gid);
			if (this->libc->lstat(name.c_str(), &st) == 0) {
				// parent directory is SGID'ed
				if (st.st_gid != getgid())
					gid = st.st_gid;
			}
			this->libc->chown(name.c_str(), uid, gid);
		}
		return 0;
	}

	return -errno;
}
int Fuse::op_rmdir(const std::string path)
{
    std::string dir;
    int res = 0;

    try{
        dir = this->findPath(path);
        res = this->libc->rmdir(dir.c_str());
        if (res == -1){ return -errno; }

        return 0;
    }catch(...){}

    return -ENOENT;
}

int Fuse::op_unlink(const std::string path)
{
    int res = 0;

    try{
	    std::string file = this->findPath(path);
        res = this->libc->unlink(file.c_str());
    }catch(...){
		errno = ENOENT;
		return -errno;
    }

	if (res == -1) return -errno;
	return 0;
}

int Fuse::op_rename(const std::string from, const std::string to)
{
    int res;
	struct stat sto, sfrom;
	int from_is_dir = 0, to_is_dir = 0, from_is_file = 0, to_is_file = 0;
	int to_dir_is_empty = 1;

	if (from == to)
		return 0;
    std::string fromParent = this->get_parent_path(from);
    std::string toParent = this->get_parent_path(to);

	std::map<std::string, std::string> &directories = this->config->option<std::map<std::string, std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

	// seek for possible errors
    std::map<std::string, std::string>::iterator dit;
	for(dit=directories.begin();dit != directories.end();dit++){
        // if virtual mountpoint 
        if(dit->second.find(fromParent)!=0 || dit->second.find(toParent)!=0){ continue; }

		std::string obj_to   = this->concatPath(dit->first, to);
		std::string obj_from = this->concatPath(dit->first, from);
		if (this->libc->stat(obj_to.c_str(), &sto) == 0) {
			if (S_ISDIR(sto.st_mode)) {
				to_is_dir++;
				if (!this->dir_is_empty(obj_to))
					to_dir_is_empty = 0;
			}
			else
				to_is_file++;
		}
		if (this->libc->stat(obj_from.c_str(), &sfrom) == 0) {
			if (S_ISDIR (sfrom.st_mode))
				from_is_dir++;
			else
				from_is_file++;
		}

		if (to_is_file && from_is_dir)
			return -ENOTDIR;
		if (to_is_file && to_is_dir)
			return -ENOTEMPTY;
		if (from_is_dir && !to_dir_is_empty)
			return -ENOTEMPTY;
	}

	// parent 'to' path doesn't exists
	std::string pto = this->get_parent_path(to);
    try{
        this->findPath(pto);
    }
    catch(...){
        return -ENOENT;
    }

	// rename cycle
	for(dit=directories.begin();dit != directories.end();dit++){
        if(dit->second.find(fromParent)!=0 || dit->second.find(toParent)!=0){ continue; }

		std::string obj_to   = this->concatPath(dit->first, to);
		std::string obj_from = this->concatPath(dit->first, from);

		if (this->libc->stat(obj_from.c_str(), &sfrom) == 0) {
			// if from is dir and at the same time file, we only rename dir
			if (from_is_dir && from_is_file) {
				if (!S_ISDIR(sfrom.st_mode)) {
					continue;
				}
			}

			this->create_parent_dirs(dit->first, to);

			res = this->libc->rename(obj_from.c_str(), obj_to.c_str());
			if (res == -1) {
				return -errno;
			}
		} else {
			// from and to are files, so we must remove to files
			if (from_is_file && to_is_file && !from_is_dir) {
				if (this->libc->stat(obj_to.c_str(), &sto) == 0) {
					if (this->libc->unlink(obj_to.c_str()) == -1) {
						return -errno;
					}
				}
			}
		}

	}

	return 0;
}

int Fuse::op_utimens(const std::string path, const struct timespec ts[2])
{
	size_t flag_found;
    int res;
    struct stat st;

	std::map<std::string, std::string> &directories = this->config->option<std::map<std::string, std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    std::map<std::string, std::string>::iterator dit;
	for(dit=directories.begin(), flag_found=0;dit != directories.end();dit++){
		std::string object = this->concatPath(dit->first, path);
		if (this->libc->lstat(object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;

        res = this->libc->utimensat(AT_FDCWD, object.c_str(), ts, AT_SYMLINK_NOFOLLOW);

		if (res == -1)
			return -errno;
	}
	if (flag_found)
		return 0;
	errno = ENOENT;
	return -errno;
}

int Fuse::op_chmod(const std::string path, mode_t mode)
{
	size_t flag_found;
    int res;
    struct stat st;

	std::map<std::string, std::string> &directories = this->config->option<std::map<std::string, std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    std::map<std::string, std::string>::iterator dit;
	for(dit=directories.begin(), flag_found=0;dit != directories.end();dit++){
		std::string object = this->concatPath(dit->first, path);
		if (this->libc->lstat(object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;
		res = this->libc->chmod(object.c_str(), mode);

		if (res == -1)
			return -errno;
	}
	if (flag_found)
		return 0;
	errno = ENOENT;
	return -errno;
}

int Fuse::op_chown(const std::string path, uid_t uid, gid_t gid)
{
	size_t flag_found;
    int res;
    struct stat st;

	std::map<std::string, std::string> &directories = this->config->option<std::map<std::string, std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    std::map<std::string, std::string>::iterator dit;
	for(dit=directories.begin(), flag_found=0;dit != directories.end();dit++){
		std::string object = this->concatPath(dit->first, path);
		if (this->libc->lstat(object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;
		res = this->libc->lchown(object.c_str(), uid, gid);
		if (res == -1)
			return -errno;
	}
	if (flag_found)
		return 0;
	errno = ENOENT;
	return -errno;
}

int Fuse::op_symlink(const std::string oldname, const std::string newname)
{
	int i, res;
    
    std::string parent, directory;
    try{
        parent = this->get_parent_path(newname);
        directory = this->findPath(parent);
    }catch(...){
        errno = ENOENT;
		return -errno;
    }

	for (i = 0; i < 2; i++) {
		if (i) {
            try{
			    directory = this->getMaxFreeSpaceDir();
            }catch(...){
				errno = ENOSPC;
				return -errno;
			}

			this->create_parent_dirs(directory, newname);
		}

		std::string path_to = this->concatPath(directory, newname);

		res = this->libc->symlink(oldname.c_str(), path_to.c_str());

		if (res == 0)
			return 0;
		if (errno != ENOSPC)
			return -errno;
	}
	return -errno;
}

int Fuse::op_link(const std::string oldname, const std::string newname)
{
    int res = 0;
    std::string usedPath;

    try{
        this->findPath(oldname, NULL, &usedPath);
    }catch(...){
		errno = ENOENT;
		return -errno;
    }

	res = this->create_parent_dirs(usedPath, newname);
	if (res != 0) {
		return res;
	}

	std::string path_oldname = this->concatPath(usedPath, oldname);
	std::string path_newname = this->concatPath(usedPath, newname);

	res = this->libc->link(path_oldname.c_str(), path_newname.c_str());

	if (res == 0)
		return 0;
	return -errno;
}

int Fuse::op_mknod(const std::string path, mode_t mode, dev_t rdev)
{
	int res, i;
    std::string parent, directory;
    try{
        parent = this->get_parent_path(path);
        directory = this->findPath(parent);
    }catch(...){
        errno = ENOENT;
		return -errno;
    }

	for (i = 0; i < 2; i++) {
		if (i) {
            try{
                directory = this->getMaxFreeSpaceDir();
            }catch(...){
				errno = ENOSPC;
				return -errno;
            }

			this->create_parent_dirs(directory, path);
		}
		std::string nod = this->concatPath(directory, path);

		if (S_ISREG(mode)) {
			res = this->libc->open(nod.c_str(), O_CREAT | O_EXCL | O_WRONLY, mode);
			if (res >= 0)
				res = close(res);
		} else if (S_ISFIFO(mode))
			res = this->libc->mkfifo(nod.c_str(), mode);
		else
			res = this->libc->mknod(nod.c_str(), mode, rdev);

		if (res != -1) {
			if (this->libc->getuid() == 0) {
                uid_t uid;
                gid_t gid;
                this->determineCaller(&uid, &gid);
				this->libc->chown(nod.c_str(), uid, gid);
			}

			return 0;
		}

		if (errno != ENOSPC)
			return -errno;
	}
	return -errno;
}

#if _POSIX_SYNCHRONIZED_IO + 0 > 0 || defined(__FreeBSD__)
#undef HAVE_FDATASYNC
#else
#define HAVE_FDATASYNC 1
#endif

int Fuse::op_fsync(const std::string path, int isdatasync, struct fuse_file_info *fi)
{
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

    int res = 0;

#ifdef HAVE_FDATASYNC
	if (isdatasync)
		res = this->libc->fdatasync(fd);
	else
#endif
		res = this->libc->fsync(fd);

	if (res == -1)
		return -errno;
	return 0;
}

#ifndef WITHOUT_XATTR
int Fuse::op_setxattr(const std::string file_name, const std::string attrname,
                const char *attrval, size_t attrvalsize, int flags)
{
	try{
		std::string path = this->findPath(file_name);
        if (this->libc->setxattr(path.c_str(), attrname.c_str(), attrval, attrvalsize, flags) == -1) return -errno;
		return 0;
	}
	catch(...){}
	return -ENOENT;
}

int Fuse::op_getxattr(const std::string file_name, const std::string attrname, char *buf, size_t count)
{
	try{
		std::string path = this->findPath(file_name);
        int size = this->libc->getxattr(path.c_str(), attrname.c_str(), buf, count);
        if(size == -1) return -errno;
		return size;
	}
	catch(...){}
	return -ENOENT;
}

int Fuse::op_listxattr(const std::string file_name, char *buf, size_t count)
{
    try{
		std::string path = this->findPath(file_name);
		int ret = 0;
        if((ret=this->libc->listxattr(path.c_str(), buf, count)) == -1) return -errno;
		return ret;
	}
	catch(...){}
	return -ENOENT;
}

int Fuse::op_removexattr(const std::string file_name, const std::string attrname)
{
	try{
		std::string path = this->findPath(file_name);
        if(this->libc->removexattr(path.c_str(), attrname.c_str()) == -1) return -errno;
		return 0;
	}
	catch(...){}
	return -ENOENT;
}
#endif




////// static functions to forward function call to Fuse* instance

void* Fuse::init(struct fuse_conn_info *conn){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->init(conn);
}
void Fuse::destroy(void *arg){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->destroy(arg);
}

int Fuse::getattr(const char *file_name, struct stat *buf)
{
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_getattr(file_name, buf);
}
int Fuse::statfs(const char *path, struct statvfs *buf)
{
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_statfs(path, buf);
}

int Fuse::readdir(const char *dirname, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info * fi){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_readdir(dirname, buf, filler, offset, fi);
}
int Fuse::readlink(const char *path, char *buf, size_t size){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_readlink(path, buf, size);
}

int Fuse::create(const char *file, mode_t mode, struct fuse_file_info *fi)
{
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_create(file, mode, fi);
}
int Fuse::open(const char *file, struct fuse_file_info *fi)
{
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_open(file, fi);
}
int Fuse::release(const char *path, struct fuse_file_info *fi)
{
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_open(path, fi);
}
int Fuse::read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_read(path, buf, count, offset, fi);
}
int Fuse::write(const char *file, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_write(file, buf, count, offset, fi);
}
int Fuse::truncate(const char *path, off_t size){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_truncate(path, size);
}
int Fuse::ftruncate(const char *path, off_t size, struct fuse_file_info *fi){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_ftruncate(path, size, fi);
}
int Fuse::access(const char *path, int mask){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_access(path, mask);
}
int Fuse::mkdir(const char *path, mode_t mode){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_mkdir(path, mode);
}
int Fuse::rmdir(const char *path){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_rmdir(path);
}
int Fuse::unlink(const char *path){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_unlink(path);
}
int Fuse::rename(const char *from, const char *to){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_rename(from, to);
}
int Fuse::utimens(const char *path, const struct timespec ts[2]){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_utimens(path, ts);
}
int Fuse::chmod(const char *path, mode_t mode){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_chmod(path, mode);
}
int Fuse::chown(const char *path, uid_t uid, gid_t gid){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_chown(path, uid, gid);
}
int Fuse::symlink(const char *from, const char *to){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_symlink(from, to);
}
int Fuse::link(const char *from, const char *to){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_link(from, to);
}
int Fuse::mknod(const char *path, mode_t mode, dev_t rdev){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_mknod(path, mode, rdev);
}
int Fuse::fsync(const char *path, int isdatasync, struct fuse_file_info *fi){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_fsync(path, isdatasync, fi);
}

int Fuse::setxattr(const char *path, const char *attrname,
					const char *attrval, size_t attrvalsize, int flags){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_setxattr(path, attrname, attrval, attrvalsize, flags);
}
int Fuse::getxattr(const char *path, const char *attrname, char *buf, size_t count){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_getxattr(path, attrname, buf, count);
}
int Fuse::listxattr(const char *path, char *buf, size_t count){
    struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_listxattr(path, buf, count);
}
int Fuse::removexattr(const char *path, const char *attrname){
	struct fuse_context *ctx = fuse_get_context();
    Fuse *instance = static_cast<Fuse*>(ctx->private_data);
    return instance->op_removexattr(path, attrname);
}


}

#endif
