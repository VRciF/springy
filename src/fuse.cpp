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

namespace Springy{

const char *Fuse::fsname = "springy";

Fuse::Fuse(){
    this->fuse = NULL;
    this->config = NULL;
}
Fuse& Fuse::init(Settings *config){
    this->config = config;

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
        ::stat(this->mountpoint.c_str(), &buf);

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
    std::cout << "filename:" << file_name << std::endl;

	std::set<std::string> &directories = this->config->option<std::set<std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    std::set<std::string>::iterator it;
	for(it=directories.begin();it != directories.end();it++){
		std::string path = this->concatPath(*it, file_name);
        std::cout << "path:" << path << std::endl;

		if(::lstat(path.c_str(), buf) != -1){
            if(usedPath!=NULL){
                usedPath->assign(*it);
            }
            return path;
        }
    }
    throw std::runtime_error("file not found");
}
std::string Fuse::getMaxFreeSpaceDir(fsblkcnt_t *space){
	std::vector<std::string> &directories = this->config->option<std::vector<std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    std::string maxFreeSpaceDir;
    fsblkcnt_t max_space = 0;
    if(space!=NULL){
        space = &max_space;
    }
    *space = 0;
	for(unsigned int i=0;i<directories.size();i++){
        struct statvfs stf;
		if (statvfs(directories[i].c_str(), &stf) != 0)
			continue;
		fsblkcnt_t curspace  = stf.f_bsize;
		curspace *= stf.f_bavail;
        
        if(curspace>*space){
            maxFreeSpaceDir = directories[i];
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
    }catch(...){ return 0; }

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
	if (::stat(path_parent.c_str(), &st)==0)
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
	if (::stat(exists.c_str(), &st)!=0)
	{
		return -errno;
	}
	res=::mkdir(path_parent.c_str(), st.st_mode);
	if (res==0)
	{
		::chown(path_parent.c_str(), st.st_uid, st.st_gid);
		::chmod(path_parent.c_str(), st.st_mode);
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
        if ((listsize=::listxattr(from.c_str(), NULL, 0)) == 0)
                return 0;

        // get all extended attributes
        listbuf=(char *)calloc(sizeof(char), listsize);
        if (::listxattr(from.c_str(), listbuf, listsize) == -1)
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
                attrvalsize = ::getxattr(from.c_str(), name_begin, NULL, 0);
                if (attrvalsize < 0)
                {
                        return -1;
                }

                // get the value of the extended attribute
                attrvalbuf=(char *)calloc(sizeof(char), attrvalsize);
                if (::getxattr(from.c_str(), name_begin, attrvalbuf, attrvalsize) < 0)
                {
                        return -1;
                }

                // set the value of the extended attribute on dest file
                if (::setxattr(to.c_str(), name_begin, attrvalbuf, attrvalsize, 0) < 0)
                {
                        return -1;
                }

                free(attrvalbuf);

                // point the pointer to the start of the attr name to the start
                // of the next attr
                name_begin=name_end+1;
                name_end++;
        }

        free(listbuf);
        return 0;
}
#endif
int Fuse::dir_is_empty(const std::string path)
{
	DIR * dir = opendir(path.c_str());
	struct dirent *de;
	if (!dir)
		return -1;
	while((de = ::readdir(dir))) {
		if (strcmp(de->d_name, ".") == 0) continue;
		if (strcmp(de->d_name, "..") == 0) continue;
		::closedir(dir);
		return 0;
	}

	::closedir(dir);
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
    for(it = range.first;range.first!=range.second;it++){
        off_t seek = lseek(it->fd, 0, SEEK_CUR);
        int flags = it->flags;
        int fh;

        flags &= ~(O_EXCL|O_TRUNC);

        // open
        if ((fh = ::open(newFile.c_str(), flags)) == -1) {
            it->valid = false;
            continue;
        }
        else
        {
            // seek
            if (seek != lseek(fh, seek, SEEK_SET)) {
                ::close(fh);
                it->valid = false;
                continue;
            }

            // filehandle
            if (dup2(fh, it->fd) != it->fd) {
                ::close(fh);
                it->valid = false;
                continue;
            }
            // close temporary filehandle
            ::close(fh);
        }

        range.first->path = newDirectory;
    }
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
        ::fclose(input);
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
        ::fclose(input);
        ::fclose(output);
        ::unlink(to.c_str());
        throw std::runtime_error("not enough memory");
    }

	while((size = ::read(inputfd, buf, sizeof(char)*allocationSize))>0) {
        ssize_t written = 0;
        while(written<=size){
            size_t bytesWritten = ::write(outputfd, buf+written, sizeof(char)*(size-written));
            if(bytesWritten>0){
                written += bytesWritten;
            }
            else{
                ::fclose(input);
                ::fclose(output);
                delete[] buf;
                ::unlink(to.c_str());
                throw std::runtime_error("moving file failed");
            }
        }
	}

    delete[] buf;
	if(size==-1){
        ::fclose(input);
        ::fclose(output);
        ::unlink(to.c_str());
        throw std::runtime_error("read error occured");
    }

	::fclose(input);

	// owner/group/permissions
	::fchmod(outputfd, st.st_mode);
	::fchown(outputfd, st.st_uid, st.st_gid);
	::fclose(output);

	// time
	ftime.actime = st.st_atime;
	ftime.modtime = st.st_mtime;
	::utime(to.c_str(), &ftime);

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
        ::unlink(to.c_str());
        throw std::runtime_error("failed to modify internal data structure");
    }

    try{
        this->reopen_files(file, maxSpaceDir);
        ::unlink(from.c_str());
    }catch(...){
        ::unlink(to.c_str());
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
	try{
		std::string path = this->findPath(file_name, buf);
		return 0;
	}
	catch(...){}

    return -ENOENT;
}

int Fuse::op_statfs(const std::string path, struct statvfs *buf)
{
	std::map<dev_t, struct statvfs> stats;
	std::map<dev_t, struct statvfs>::iterator it;
	std::vector<std::string> &directories = this->config->option<std::vector<std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    unsigned long min_block = 0, min_frame = 0;

	for(unsigned int i=0;i<directories.size();i++){
		struct stat st;
		int ret = stat(directories[i].c_str(), &st);
		if (ret != 0) {
			return -errno;
		}
		if(stats.find(st.st_dev)!=stats.end()){ continue; }
		
		struct statvfs stv;
		ret = ::statvfs(directories[i].c_str(), &stv);
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
	size_t i, found;
    std::vector<std::string> dirs;

	std::vector<std::string> &directories = this->config->option<std::vector<std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

	// find all dirs
    for (found = i = 0; i < directories.size(); i++) {
		std::string path = this->concatPath(directories[i], dirname);
		if (stat(path.c_str(), &st) == 0) {
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
	for (int i = 0; dirs.size(); i++) {
		struct dirent *de;
		DIR * dh = opendir(dirs[i].c_str());
		if (!dh)
			continue;

		while((de = ::readdir(dh))) {
			// find dups
            if(dir_item_ht.find(de->d_name)!=dir_item_ht.end()){
                continue;
            }

			// add item
            std::string object_name = this->concatPath(dirs[i], de->d_name);

            lstat(object_name.c_str(), &st);
            dir_item_ht.insert(std::make_pair(de->d_name, st));
		}

		closedir(dh);
	}

    for(it=dir_item_ht.begin();it!=dir_item_ht.end();it++){
        if (filler(buf, it->first.c_str(), &it->second, 0))
                break;
    }

	return 0;
}

int Fuse::op_readlink(const std::string path, char *buf, size_t size)
{
    int res = 0;
    try{
        std::string link = this->findPath(path);
        memset(buf, 0, size);
        res = ::readlink(link.c_str(), buf, size);

        if (res >= 0)
            return 0;
        return -errno;
    }
    catch(...){}
	return -ENOENT;
}

int Fuse::internal_open(const std::string file, mode_t mode, struct fuse_file_info *fi)
{
    fi->fh = 0;

    try{
        int fd = 0;
        std::string usedPath;
	    std::string path = this->findPath(file, NULL, &usedPath);
		if (mode != 0)
			fd = ::open(path.c_str(), fi->flags, mode);
		else
			fd = ::open(path.c_str(), fi->flags);
		if (fd == -1) {
			return -errno;
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

            struct openFile of = { file, usedPath, fd, fi->flags, syncToken, true};
            this->openFiles.insert(of);
        }
        catch(...){
            //log boost::current_exception_diagnostic_information()
            if(errno==0){ errno = ENOMEM; }
            int rval = errno;
            ::close(fd);
            return -rval;
        }

        fi->fh = fd;

		return 0;
    }
    catch(...){
        //log boost::current_exception_diagnostic_information()
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
	if (mode != 0)
		fd = ::open(path.c_str(), fi->flags, mode);
	else
		fd = ::open(path.c_str(), fi->flags);

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
        //log boost::current_exception_diagnostic_information()
        if(errno==0){ errno = ENOMEM; }
        int rval = errno;
        ::close(fd);
        return -rval;
    }

    fi->fh = fd;

	return 0;
}

int Fuse::op_create(const std::string file, mode_t mode, struct fuse_file_info *fi)
{
    int res = 0;
	res = this->internal_open(file, mode, fi);
	return res;
}

int Fuse::op_open(const std::string file, struct fuse_file_info *fi)
{
    int res = 0;
	res = this->internal_open(file, 0, fi);
	return res;
}

int Fuse::op_release(const std::string path, struct fuse_file_info *fi)
{
    int fd = fi->fh;

    try{
        Synchronized syncOpenFiles(this->openFiles);
        ::close(fd);

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
        //log boost::current_exception_diagnostic_information()
        if(errno==0){ errno = EBADFD; }
        return -errno;
    }

	return 0;
}

int Fuse::op_read(const std::string, char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
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

	ssize_t res;
	res = pread(fd, buf, count, offset);

	if (res == -1)
		return -errno;
	return res;
}

int Fuse::op_write(const std::string file, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
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
    }catch(...){}

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
        //log boost::current_exception_diagnostic_information()
        if(errno==0){ errno = EBADFD; }
        return -errno;
    }

    Synchronized sync(syncToken);

	res = ::pwrite(fd, buf, count, offset);
	if ((res == (ssize_t)count) || (res == -1 && errno != ENOSPC)) {
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
        return -errno;
    }

	if (res == 0) {
        res = ::pwrite(fd, buf, count, offset);

		if (res == -1) {
			return -errno;
		}

		return res;
	}
	errno = ENOSPC;

	return -errno;
}

int Fuse::op_truncate(const std::string path, off_t size)
{
    try{
        std::string file = this->findPath(path);
        int res = ::truncate(file.c_str(), size);

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

	if (::ftruncate(fd, size) == -1)
		return -errno;
	return 0;
}

int Fuse::op_access(const std::string path, int mask)
{
    try{
        std::string file = this->findPath(path);
        int res = ::access(file.c_str(), mask);

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
	if (::mkdir(name.c_str(), mode) == 0) {
		if (getuid() == 0) {
			struct stat st;
            gid_t gid;
            uid_t uid;
            this->determineCaller(&uid, &gid);
			if (::lstat(name.c_str(), &st) == 0) {
				// parent directory is SGID'ed
				if (st.st_gid != getgid())
					gid = st.st_gid;
			}
			::chown(name.c_str(), uid, gid);
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
        while(true) {
            dir = this->findPath(path);
            res = ::rmdir(dir.c_str());
            if (res == -1) return -errno;
        }
    }catch(...){}

	return 0;
}

int Fuse::op_unlink(const std::string path)
{
    int res = 0;

    try{
	    std::string file = this->findPath(path);
        res = ::unlink(file.c_str());
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

	std::vector<std::string> &directories = this->config->option<std::vector<std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

	// seek for possible errors
    for (size_t i = 0; i < directories.size(); i++) {
		std::string obj_to   = this->concatPath(directories[i], to);
		std::string obj_from = this->concatPath(directories[i], from);
		if (stat(obj_to.c_str(), &sto) == 0) {
			if (S_ISDIR(sto.st_mode)) {
				to_is_dir++;
				if (!this->dir_is_empty(obj_to))
					to_dir_is_empty = 0;
			}
			else
				to_is_file++;
		}
		if (stat(obj_from.c_str(), &sfrom) == 0) {
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
	for (size_t i = 0; i < directories.size(); i++) {
		std::string obj_to   = this->concatPath(directories[i], to);
		std::string obj_from = this->concatPath(directories[i], from);

		if (::stat(obj_from.c_str(), &sfrom) == 0) {
			// if from is dir and at the same time file, we only rename dir
			if (from_is_dir && from_is_file) {
				if (!S_ISDIR(sfrom.st_mode)) {
					continue;
				}
			}

			this->create_parent_dirs(directories[i], to);

			res = ::rename(obj_from.c_str(), obj_to.c_str());
			if (res == -1) {
				return -errno;
			}
		} else {
			// from and to are files, so we must remove to files
			if (from_is_file && to_is_file && !from_is_dir) {
				if (::stat(obj_to.c_str(), &sto) == 0) {
					if (::unlink(obj_to.c_str()) == -1) {
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
	size_t i, flag_found;
    int res;
    struct timeval tv[2];
    struct stat st;

	std::vector<std::string> &directories = this->config->option<std::vector<std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    for (flag_found = i = 0; i < directories.size(); i++) {
		std::string object = this->concatPath(directories[i], path);
		if (::lstat(object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;

		tv[0].tv_sec = ts[0].tv_sec;
		tv[0].tv_usec = ts[0].tv_nsec / 1000;
		tv[1].tv_sec = ts[1].tv_sec;
		tv[1].tv_usec = ts[1].tv_nsec / 1000;

		res = ::lutimes(object.c_str(), tv);

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
	size_t i, flag_found;
    int res;
    struct stat st;

	std::vector<std::string> &directories = this->config->option<std::vector<std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    for (flag_found = i = 0; i < directories.size(); i++) {
		std::string object = this->concatPath(directories[i], path);
		if (::lstat(object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;
		res = ::chmod(object.c_str(), mode);

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
	size_t i, flag_found;
    int res;
    struct stat st;

	std::vector<std::string> &directories = this->config->option<std::vector<std::string> >("directories");
	Synchronized sDirs(directories, Synchronized::LockType::READ);

    for (flag_found = i = 0; i < directories.size(); i++) {
		std::string object = this->concatPath(directories[i], path);
		if (::lstat(object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;
		res = ::lchown(object.c_str(), uid, gid);
		if (res == -1)
			return -errno;
	}
	if (flag_found)
		return 0;
	errno = ENOENT;
	return -errno;
}

int Fuse::op_symlink(const std::string from, const std::string to)
{
	int i, res;
    
    std::string parent, directory;
    try{
        parent = this->get_parent_path(to);
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

			this->create_parent_dirs(directory, to);
		}

		std::string path_to = this->concatPath(directory, to);

		res = symlink(from.c_str(), path_to.c_str());

		if (res == 0)
			return 0;
		if (errno != ENOSPC)
			return -errno;
	}
	return -errno;
}

int Fuse::op_link(const std::string from, const std::string to)
{
    int res = 0;
    std::string directory;

    try{
        directory = this->findPath(from);
    }catch(...){
		errno = ENOENT;
		return -errno;
    }

	res = this->create_parent_dirs(directory, to);
	if (res != 0) {
		return res;
	}

	std::string path_from = this->concatPath(directory, from);
	std::string path_to = this->concatPath(directory, to);

	res = ::link(path_from.c_str(), path_to.c_str());

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
			res = ::open(nod.c_str(), O_CREAT | O_EXCL | O_WRONLY, mode);
			if (res >= 0)
				res = close(res);
		} else if (S_ISFIFO(mode))
			res = ::mkfifo(nod.c_str(), mode);
		else
			res = ::mknod(nod.c_str(), mode, rdev);

		if (res != -1) {
			if (::getuid() == 0) {
                uid_t uid;
                gid_t gid;
                this->determineCaller(&uid, &gid);
				::chown(nod.c_str(), uid, gid);
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
		res = ::fdatasync(fd);
	else
#endif
		res = ::fsync(fd);

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
        if (::setxattr(path.c_str(), attrname.c_str(), attrval, attrvalsize, flags) == -1) return -errno;
		return 0;
	}
	catch(...){}
	return -ENOENT;
}

int Fuse::op_getxattr(const std::string file_name, const std::string attrname, char *buf, size_t count)
{
	try{
		std::string path = this->findPath(file_name);
        int size = ::getxattr(path.c_str(), attrname.c_str(), buf, count);
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
        if((ret=::listxattr(path.c_str(), buf, count)) == -1) return -errno;
		return ret;
	}
	catch(...){}
	return -ENOENT;
}

int Fuse::op_removexattr(const std::string file_name, const std::string attrname)
{
	try{
		std::string path = this->findPath(file_name);
        if(::removexattr(path.c_str(), attrname.c_str()) == -1) return -errno;
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
