#ifdef HAS_FUSE

#include "fuse.hpp"

#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include <set>
#include <boost/algorithm/string/join.hpp>

namespace Springy{

const char *Fuse::fsname = "springy";

Fuse::Fuse(){
    this->fuse = NULL;
    this->config = NULL;
}
Fuse& Fuse::init(Settings *config){
    this->config = config;

    this->fops.init	   = Fuse::init;
    this->fops.destroy = Fuse::destroy;
    this->fops.getattr = Fuse::getattr;


    /*
            fops.getattr    	= mhdd_stat;
            fops.statfs     	= mhdd_statfs;
            fops.readdir    	= mhdd_readdir;
            fops.readlink   	= mhdd_readlink;
            fops.open       	= mhdd_fileopen;
            fops.release    	= mhdd_release;
            fops.read       	= mhdd_read;
            fops.write      	= mhdd_write;
            fops.create     	= mhdd_create;
            fops.truncate   	= mhdd_truncate;
            fops.ftruncate  	= mhdd_ftruncate;
            fops.access     	= mhdd_access;
            fops.mkdir      	= mhdd_mkdir;
            fops.rmdir      	= mhdd_rmdir;
            fops.unlink     	= mhdd_unlink;
            fops.rename     	= mhdd_rename;
            fops.utimens    	= mhdd_utimens;
            fops.chmod      	= mhdd_chmod;
            fops.chown      	= mhdd_chown;
            fops.symlink    	= mhdd_symlink;
            fops.mknod      	= mhdd_mknod;
            fops.fsync      	= mhdd_fsync;
            fops.link		    = mhdd_link;
        #ifndef WITHOUT_XATTR
                fops.setxattr   	= mhdd_setxattr;
                fops.getxattr   	= mhdd_getxattr;
                fops.listxattr  	= mhdd_listxattr;
                fops.removexattr	= mhdd_removexattr;
        #endif
    */

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

    std::set<std::string> &options = this->config->option<std::set<std::string> >("options");
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
	    fuse_exit(this->fuse);
	}

	struct stat buf;
	::stat(this->mountpoint.c_str(), &buf);

    // wait for 
    try{
        this->th.join();
    }catch(...){}

    return *this;
}

void Fuse::thread(Fuse *instance){
    int res = 0;
    instance->thrunning = 1;

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
    instance->thrunning = 0;
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




void* Fuse::op_init(struct fuse_conn_info *conn){
    return static_cast<void*>(this);
}
void Fuse::op_destroy(void *arg){}

int Fuse::op_getattr(const std::string file_name, struct stat *buf)
{
/*
    std::string path = find_path(file_name);
	if (path.size()) {
		int ret = ::lstat(path.c_str(), buf);

		if (ret == -1) return -errno;
		return 0;
	}
	errno = ENOENT;
	return -errno;
*/    
    return -ENOENT;
}

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

/*
static int mhdd_statfs(const char *path, struct statvfs *buf)
{
	size_t i, j;
    std::vector<struct statvfs> stats;
    std::vector<dev_t> devices;

	struct stat st;

    unsigned long min_block = 0, min_frame = 0;

	mhdd_debug(MHDD_MSG, "mhdd_statfs: %s\n", path);

    stats.resize(mhdd->dirs.size());
    devices.resize(mhdd->dirs.size());

	for (i = 0; i < mhdd->dirs.size(); i++) {
		int ret = statvfs(mhdd->dirs[i].c_str(), &stats[i]);
		if (ret != 0) {
			return -errno;
		}

		ret = stat(mhdd->dirs[i].c_str(), &st);
		if (ret != 0) {
			return -errno;
		}
		devices[i] = st.st_dev;
	}

    min_block = stats[0].f_bsize,
    min_frame = stats[0].f_frsize;

    for (i = 1; i < mhdd->dirs.size(); i++) {
		if (min_block>stats[i].f_bsize) min_block = stats[i].f_bsize;
		if (min_frame>stats[i].f_frsize) min_frame = stats[i].f_frsize;
	}

	if (!min_block)
		min_block = 512;
	if (!min_frame)
		min_frame = 512;

    for (i = 0; i < mhdd->dirs.size(); i++) {
		if (stats[i].f_bsize>min_block) {
			stats[i].f_bfree    *=  stats[i].f_bsize/min_block;
			stats[i].f_bavail   *=  stats[i].f_bsize/min_block;
			stats[i].f_bsize    =   min_block;
		}
		if (stats[i].f_frsize>min_frame) {
			stats[i].f_blocks   *=  stats[i].f_frsize/min_frame;
			stats[i].f_frsize   =   min_frame;
		}
	}

	memcpy(buf, &stats[0], sizeof(struct statvfs));

    for (i = 1; i < mhdd->dirs.size(); i++) {
		// if the device already processed, skip it
		if (devices[i]) {
			int dup_found = 0;
			for (j = 0; j < i; j++) {
				if (devices[j] == devices[i]) {
					dup_found = 1;
					break;
				}
			}

			if (dup_found)
				continue;
		}

		if (buf->f_namemax<stats[i].f_namemax) {
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

static int mhdd_readdir(
		const char *dirname,
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

	mhdd_debug(MHDD_MSG, "mhdd_readdir: %s\n", dirname);

	// find all dirs
    for (found = i = 0; i < mhdd->dirs.size(); i++) {
		std::string path = create_path(mhdd->dirs[i], dirname);
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

		while((de = readdir(dh))) {
			// find dups
            if(dir_item_ht.find(de->d_name)!=dir_item_ht.end()){
                continue;
            }

			// add item
            std::string object_name = create_path(dirs[i], de->d_name);

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

static int mhdd_readlink(const char *path, char *buf, size_t size)
{
    int res = 0;

	mhdd_debug(MHDD_MSG, "mhdd_readlink: %s, size = %d\n", path, size);

	std::string link = find_path(path);
	if (link.size()) {
		memset(buf, 0, size);
		res = readlink(link.c_str(), buf, size);

		if (res >= 0)
			return 0;
	}
	return -1;
}

#define CREATE_FUNCTION 0
#define OPEN_FUNCION    1

static int mhdd_internal_open(const char *file,
		mode_t mode, struct fuse_file_info *fi, int what)
{
	int dir_id, fd;

	std::string path;
    struct openFile *add = NULL;

	mhdd_debug(MHDD_INFO, "mhdd_internal_open: %s, flags = 0x%X, fi: %p\n",
		file, fi->flags, fi);

    fi->fh = 0;

	path = find_path(file);
	if (path.size()) {
		if (what == CREATE_FUNCTION)
			fd = open(path.c_str(), fi->flags, mode);
		else
			fd = open(path.c_str(), fi->flags);
		if (fd == -1) {
			return -errno;
		}
		add = flist_create(file, path, fi->flags, fd);

        if(add==NULL){
            if(errno==0){ errno = ENOMEM; }
            return -errno;
        }

        fi->fh = reinterpret_cast<uint64_t>(add);

        mhdd_debug(MHDD_INFO, "mhdd_internal_open: file opened %d\n", fi->fh);
        mhdd_debug(MHDD_DEBUG, "mhdd_open: add->fh: %p:%lld\n", add, add->fh);

		return 0;
	}

	mhdd_debug(MHDD_INFO, "mhdd_internal_open: new file %s\n", file);

	if ((dir_id = get_free_dir()) < 0) {
		errno = ENOSPC;
		return -errno;
	}

	create_parent_dirs(dir_id, file);
	path = create_path(mhdd->dirs[dir_id], file);

	if (what == CREATE_FUNCTION)
		fd = open(path.c_str(), fi->flags, mode);
	else
		fd = open(path.c_str(), fi->flags);

	if (fd == -1) {
		return -errno;
	}

	if (getuid() == 0) {
		struct stat st;
		gid_t gid = fuse_get_context()->gid;
		if (fstat(fd, &st) == 0) {
			// parent directory is SGID'ed
			if (st.st_gid != getgid()) gid = st.st_gid;
		}
		fchown(fd, fuse_get_context()->uid, gid);
	}

    add = flist_create(file, path, fi->flags, fd);

    if(add==NULL){
        if(errno==0){ errno = ENOMEM; }
        return -errno;
    }

    fi->fh = reinterpret_cast<uint64_t>(add);

    mhdd_debug(MHDD_INFO, "mhdd_internal_open: file opened %d\n", fi->fh);
    mhdd_debug(MHDD_DEBUG, "mhdd_open: add->fh: %p:%lld\n", add, add->fh);

	return 0;
}

static int mhdd_create(const char *file,
		mode_t mode, struct fuse_file_info *fi)
{
    int res = 0;
	mhdd_debug(MHDD_MSG, "mhdd_create: %s, mode = %X\n", file, fi->flags);
	res = mhdd_internal_open(file, mode, fi, CREATE_FUNCTION);
	if (res != 0)
		mhdd_debug(MHDD_INFO,
			"mhdd_create: error: %s\n", strerror(-res));
	return res;
}

static int mhdd_fileopen(const char *file, struct fuse_file_info *fi)
{
    int res = 0;
	mhdd_debug(MHDD_MSG,
		"mhdd_fileopen: %s, flags = %04X\n", file, fi->flags);
	res = mhdd_internal_open(file, 0, fi, OPEN_FUNCION);
	if (res != 0)
		mhdd_debug(MHDD_INFO,
			"mhdd_fileopen: error: %s\n", strerror(-res));
	return res;
}

static int mhdd_release(const char *path, struct fuse_file_info *fi)
{
	struct openFile *del = reinterpret_cast<struct openFile *>(fi->fh);

	mhdd_debug(MHDD_MSG, "mhdd_release: %s, handle = %p:%lld\n", path, fi, fi->fh);

    if(!del){
		mhdd_debug(MHDD_INFO,
			"mhdd_release: unknown file number: %llu\n", fi->fh);
		errno = EBADF;

		return -errno;
    }

    mhdd_debug(MHDD_DEBUG, "mhdd_release: release: %p\n", del);
    mhdd_debug(MHDD_DEBUG, "mhdd_release: release->fh: %lld\n", del->fh);

	close(del->fh);

    flist_delete(del);

	return 0;
}

static int mhdd_read(const char *path, char *buf, size_t count, off_t offset,
		struct fuse_file_info *fi)
{
	ssize_t res;
	struct openFile *info = reinterpret_cast<struct openFile *>(fi->fh);

	mhdd_debug(MHDD_INFO, "mhdd_read: %s, offset = %lld, count = %lld, handle: %p:%lld\n",
			path,
			offset,
			count,
            fi, fi->fh
		  );

    mhdd_debug(MHDD_DEBUG, "mhdd_read: openFile: %p:%d\n", info, info->fh);

	res = pread(info->fh, buf, count, offset);

	if (res == -1)
		return -errno;
	return res;
}

static int mhdd_write(const char *path, const char *buf, size_t count,
		off_t offset, struct fuse_file_info *fi)
{
	ssize_t res;
	struct openFile *info = reinterpret_cast<struct openFile *>(fi->fh);
    std::string realNameFirst, realNameSecond;

	mhdd_debug(MHDD_INFO, "mhdd_write: %s, handle = %lld\n", path, fi->fh);

    Lock openFilesLock(info->parent->lock);
    openFilesLock.rdlock();

    realNameFirst = info->parent->real_name;

	res = pwrite(info->fh, buf, count, offset);
	if ((res == (ssize_t)count) || (res == -1 && errno != ENOSPC)) {
		if (res == -1) {
			mhdd_debug(MHDD_DEBUG,
				"mhdd_write: error write %s: %s\n",
				info->parent->real_name.c_str(), strerror(errno));
			return -errno;
		}
		return res;
	}

	// end free space
    openFilesLock.wrlock();

    realNameSecond = info->parent->real_name;
    if(realNameFirst==realNameSecond){  // if the real file didnt change during requesting write uplock
        res  = move_file(info, offset + count);
    }
    openFilesLock.unlock();

	if (res == 0) {
        res = pwrite(info->fh, buf, count, offset);

		if (res == -1) {
			mhdd_debug(MHDD_DEBUG,
				"mhdd_write: error restart write: %s\n",
				strerror(errno));
			return -errno;
		}
		if (res < (ssize_t)count) {
			mhdd_debug(MHDD_DEBUG,
				"mhdd_write: error (re)write file %s %s\n",
				info->parent->real_name.c_str(),
				strerror(ENOSPC));
		}
		return res;
	}
	errno = ENOSPC;

	return -errno;
}

static int mhdd_truncate(const char *path, off_t size)
{
	std::string file = find_path(path);
	mhdd_debug(MHDD_MSG, "mhdd_truncate: %s\n", path);
	if (file.size()) {
		int res = truncate(file.c_str(), size);

		if (res == -1)
			return -errno;
		return 0;
	}
	errno = ENOENT;
	return -errno;
}

static int mhdd_ftruncate(const char *path, off_t size,
		struct fuse_file_info *fi)
{
	int res;
	struct openFile *info = reinterpret_cast<struct openFile *>(fi->fh);

	mhdd_debug(MHDD_MSG,
		"mhdd_ftruncate: %s, handle = %lld\n", path, fi->fh);

	res = ftruncate(info->fh, size);

	if (res == -1)
		return -errno;
	return 0;
}

static int mhdd_access(const char *path, int mask)
{
	mhdd_debug(MHDD_MSG, "mhdd_access: %s mode = %04X\n", path, mask);
	std::string file = find_path(path);
	if (file.size()) {
		int res = access(file.c_str(), mask);

		if (res == -1)
			return -errno;
		return 0;
	}

	errno = ENOENT;
	return -errno;
}

static int mhdd_mkdir(const char * path, mode_t mode)
{
    int dir_id = 0;

	mhdd_debug(MHDD_MSG, "mhdd_mkdir: %s mode = %04X\n", path, mode);

	if (find_path_id(path) != -1) {
		errno = EEXIST;
		return -errno;
	}

	std::string parent = get_parent_path(path);
	if (parent.size() == 0) {
		errno = EFAULT;
		return -errno;
	}

	if (find_path_id(parent) == -1) {
		errno = EFAULT;
		return -errno;
	}

	dir_id = get_free_dir();
	if (dir_id<0) {
		errno = ENOSPC;
		return -errno;
	}

	create_parent_dirs(dir_id, path);
	std::string name = create_path(mhdd->dirs[dir_id], path);
	if (mkdir(name.c_str(), mode) == 0) {
		if (getuid() == 0) {
			struct stat st;
			gid_t gid = fuse_get_context()->gid;
			if (lstat(name.c_str(), &st) == 0) {
				// parent directory is SGID'ed
				if (st.st_gid != getgid())
					gid = st.st_gid;
			}
			chown(name.c_str(), fuse_get_context()->uid, gid);
		}
		return 0;
	}

	return -errno;
}

static int mhdd_rmdir(const char * path)
{
    std::string dir;
    int res = 0;

	mhdd_debug(MHDD_MSG, "mhdd_rmdir: %s\n", path);
	
	while((dir = find_path(path)).size()) {
		res = rmdir(dir.c_str());
		if (res == -1) return -errno;
	}
	return 0;
}

static int mhdd_unlink(const char *path)
{
    int res = 0;

	mhdd_debug(MHDD_MSG, "mhdd_unlink: %s\n", path);
	std::string file = find_path(path);
	if (file.size() == 0) {
		errno = ENOENT;
		return -errno;
	}
	res = unlink(file.c_str());

	if (res == -1) return -errno;
	return 0;
}

static int mhdd_rename(const char *from, const char *to)
{
	size_t i;
    int res;
	struct stat sto, sfrom;
	int from_is_dir = 0, to_is_dir = 0, from_is_file = 0, to_is_file = 0;
	int to_dir_is_empty = 1;

	mhdd_debug(MHDD_MSG, "mhdd_rename: from = %s to = %s\n", from, to);

	if (strcmp(from, to) == 0)
		return 0;

	// seek for possible errors
    for (i = 0; i < mhdd->dirs.size(); i++) {
		std::string obj_to   = create_path(mhdd->dirs[i], to);
		std::string obj_from = create_path(mhdd->dirs[i], from);
		if (stat(obj_to.c_str(), &sto) == 0) {
			if (S_ISDIR(sto.st_mode)) {
				to_is_dir++;
				if (!dir_is_empty(obj_to))
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
	std::string pto = get_parent_path (to);
	if (find_path_id(pto) == -1) {
		return -ENOENT;
	}

	// rename cycle
	for (i = 0; i < mhdd->dirs.size(); i++) {
		std::string obj_to   = create_path(mhdd->dirs[i], to);
		std::string obj_from = create_path(mhdd->dirs[i], from);
		if (stat(obj_from.c_str(), &sfrom) == 0) {
			// if from is dir and at the same time file, we only rename dir
			if (from_is_dir && from_is_file) {
				if (!S_ISDIR(sfrom.st_mode)) {
					continue;
				}
			}

			create_parent_dirs(i, to);

			mhdd_debug(MHDD_MSG, "mhdd_rename: rename %s -> %s\n",
				obj_from.c_str(), obj_to.c_str());
			res = rename(obj_from.c_str(), obj_to.c_str());
			if (res == -1) {
				return -errno;
			}
		} else {
			// from and to are files, so we must remove to files
			if (from_is_file && to_is_file && !from_is_dir) {
				if (stat(obj_to.c_str(), &sto) == 0) {
					mhdd_debug(MHDD_MSG,
						"mhdd_rename: unlink %s\n",
						obj_to.c_str());
					if (unlink(obj_to.c_str()) == -1) {
						return -errno;
					}
				}
			}
		}

	}

	return 0;
}

static int mhdd_utimens(const char *path, const struct timespec ts[2])
{
	size_t i, flag_found;
    int res;
    struct timeval tv[2];
    struct stat st;
	mhdd_debug(MHDD_MSG, "mhdd_utimens: %s\n", path);

    for (flag_found = i = 0; i < mhdd->dirs.size(); i++) {
		std::string object = create_path(mhdd->dirs[i], path);
		if (lstat(object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;

		tv[0].tv_sec = ts[0].tv_sec;
		tv[0].tv_usec = ts[0].tv_nsec / 1000;
		tv[1].tv_sec = ts[1].tv_sec;
		tv[1].tv_usec = ts[1].tv_nsec / 1000;

		res = lutimes(object.c_str(), tv);

		if (res == -1)
			return -errno;
	}
	if (flag_found)
		return 0;
	errno = ENOENT;
	return -errno;
}

static int mhdd_chmod(const char *path, mode_t mode)
{
	size_t i, flag_found;
    int res;
    struct stat st;
	mhdd_debug(MHDD_MSG, "mhdd_chmod: mode = 0x%03X %s\n", mode, path);

    for (flag_found = i = 0; i < mhdd->dirs.size(); i++) {
		std::string object = create_path(mhdd->dirs[i], path);
		if (lstat(object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;
		res = chmod(object.c_str(), mode);

		if (res == -1)
			return -errno;
	}
	if (flag_found)
		return 0;
	errno = ENOENT;
	return -errno;
}

static int mhdd_chown(const char *path, uid_t uid, gid_t gid)
{
	size_t i, flag_found;
    int res;
    struct stat st;

	mhdd_debug(MHDD_MSG,
		"mhdd_chown: pid = 0x%03X gid = %03X %s\n", uid, gid, path);

    for (flag_found = i = 0; i < mhdd->dirs.size(); i++) {
		std::string object = create_path(mhdd->dirs[i], path);
		if (lstat(object.c_str(), &st) != 0) {
			continue;
		}

		flag_found = 1;
		res = lchown(object.c_str(), uid, gid);
		if (res == -1)
			return -errno;
	}
	if (flag_found)
		return 0;
	errno = ENOENT;
	return -errno;
}

static int mhdd_symlink(const char *from, const char *to)
{
	int i, res, dir_id;
	mhdd_debug(MHDD_MSG, "mhdd_symlink: from = %s to = %s\n", from, to);

	std::string parent = get_parent_path(to);
	if (parent.size() == 0) {
		errno = ENOENT;
		return -errno;
	}

	dir_id = find_path_id(parent);

	if (dir_id == -1) {
		errno = ENOENT;
		return -errno;
	}

	for (i = 0; i < 2; i++) {
		if (i) {
			if ((dir_id = get_free_dir()) < 0) {
				errno = ENOSPC;
				return -errno;
			}

			create_parent_dirs(dir_id, to);
		}

		std::string path_to = create_path(mhdd->dirs[dir_id], to);

		res = symlink(from, path_to.c_str());

		if (res == 0)
			return 0;
		if (errno != ENOSPC)
			return -errno;
	}
	return -errno;
}

static int mhdd_link(const char *from, const char *to)
{
    int dir_id = 0, res = 0;
	mhdd_debug(MHDD_MSG, "mhdd_link: from = %s to = %s\n", from, to);

	dir_id = find_path_id(from);

	if (dir_id == -1) {
		errno = ENOENT;
		return -errno;
	}

	res = create_parent_dirs(dir_id, to);
	if (res != 0) {
		return res;
	}

	std::string path_from = create_path(mhdd->dirs[dir_id], from);
	std::string path_to = create_path(mhdd->dirs[dir_id], to);

	res = link(path_from.c_str(), path_to.c_str());

	if (res == 0)
		return 0;
	return -errno;
}

static int mhdd_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res, i, dir_id;
    struct fuse_context * fcontext = NULL;

	mhdd_debug(MHDD_MSG, "mhdd_mknod: path = %s mode = %X\n", path, mode);
    std::string parent = get_parent_path(path);
	if (parent.size() == 0) {
		errno = ENOENT;
		return -errno;
	}

	dir_id = find_path_id(parent);

	if (dir_id == -1) {
		errno = ENOENT;
		return -errno;
	}

	for (i = 0; i < 2; i++) {
		if (i) {
			if ((dir_id = get_free_dir())<0) {
				errno = ENOSPC;
				return -errno;
			}
			create_parent_dirs(dir_id, path);
		}
		std::string nod = create_path(mhdd->dirs[dir_id], path);

		if (S_ISREG(mode)) {
			res = open(nod.c_str(), O_CREAT | O_EXCL | O_WRONLY, mode);
			if (res >= 0)
				res = close(res);
		} else if (S_ISFIFO(mode))
			res = mkfifo(nod.c_str(), mode);
		else
			res = mknod(nod.c_str(), mode, rdev);

		if (res != -1) {
			if (getuid() == 0) {
				fcontext = fuse_get_context();
				chown(nod.c_str(), fcontext->uid, fcontext->gid);
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

static int mhdd_fsync(const char *path, int isdatasync,
		struct fuse_file_info *fi)
{
	struct openFile *info = reinterpret_cast<struct openFile *>(fi->fh);;
    int res;

	mhdd_debug(MHDD_MSG,
		"mhdd_fsync: path = %s handle = %llu\n", path, fi->fh);

#ifdef HAVE_FDATASYNC
	if (isdatasync)
		res = fdatasync(info->fh);
	else
#endif
		res = fsync(info->fh);

	if (res == -1)
		return -errno;
	return 0;
}

#ifndef WITHOUT_XATTR
static int mhdd_setxattr(const char *path, const char *attrname,
                const char *attrval, size_t attrvalsize, int flags)
{
	std::string real_path = find_path(path);
    int res = 0;

	if (real_path.size() == 0)
		return -ENOENT;

	mhdd_debug(MHDD_MSG,
		"mhdd_setxattr: path = %s name = %s value = %s size = %d\n",
                real_path.c_str(), attrname, attrval, attrvalsize);
    res = setxattr(real_path.c_str(), attrname, attrval, attrvalsize, flags);

    if (res == -1) return -errno;
    return 0;
}

static int mhdd_getxattr(const char *path, const char *attrname, char *buf, size_t count)
{
    int size = 0;
	std::string real_path = find_path(path);
	if (real_path.size() == 0)
		return -ENOENT;

	mhdd_debug(MHDD_MSG,
		"mhdd_getxattr: path = %s name = %s bufsize = %d\n",
                real_path.c_str(), attrname, count);
    size = getxattr(real_path.c_str(), attrname, buf, count);

    if (size == -1) return -errno;
    return size;
}

static int mhdd_listxattr(const char *path, char *buf, size_t count)
{
    int ret = 0;
	std::string real_path = find_path(path);
	if (real_path.size() == 0)
		return -ENOENT;

	mhdd_debug(MHDD_MSG,
		"mhdd_listxattr: path = %s bufsize = %d\n",
                real_path.c_str(), count);

    ret=listxattr(real_path.c_str(), buf, count);

    if (ret == -1) return -errno;
    return ret;
}

static int mhdd_removexattr(const char *path, const char *attrname)
{
    int res = 0;
	std::string real_path = find_path(path);
	if (real_path.size() == 0)
		return -ENOENT;

	mhdd_debug(MHDD_MSG,
		"mhdd_removexattr: path = %s name = %s\n",
                real_path.c_str(), attrname);

    res = removexattr(real_path.c_str(), attrname);

    if (res == -1) return -errno;
    return 0;
}
#endif

void* mhdd_init(struct fuse_conn_info *conn){
    return NULL;
}
void mhdd_destroy(void *arg){
    mhddfs_httpd_stopServer();
}

static void mhdd_termination_handler(int signum, siginfo_t *siginfo, void *context)
{
    if(signum==SIGHUP){ return; }

	mhdd_debug(MHDD_MSG, "mhdd_termination_handler: signal received %d\n", signum);

    if(!fuse_exited(mhdd->fuse)){
	    fuse_exit(mhdd->fuse);
	}
}
*/

}

#endif
