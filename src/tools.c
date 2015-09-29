/*
   mhddfs - Multi HDD [FUSE] File System
   Copyright (C) 2008 Dmitry E. Oboukhov <dimka@avanto.org>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Modified by Glenn Washburn <gwashburn@Crossroads.com>
	   (added support for extended attributes.)
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <utime.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>

#ifndef WITHOUT_XATTR
#include <attr/xattr.h>
#endif

#include <string>
#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>

#include "tools.h"
#include "debug.h"
#include "parse_options.h"

jmp_buf uthash_descs_buf__;

/* find mount point with free space > size*/
/* -1 if not found */
static int find_free_space(off_t size)
{
	size_t i, max;
	struct statvfs stf;
	fsblkcnt_t max_space=0;
    fsblkcnt_t space = 0;

	for (max=-1,i=0; i < mhdd->dirs.size(); i++){
		if (statvfs(mhdd->dirs[i].c_str(), &stf)!=0) continue;
		space = stf.f_bsize;
		space *= stf.f_bavail;

		if (mhdd->move_limit != 0 && space > (fsblkcnt_t)(size+mhdd->move_limit)) return i;

		if (space > (fsblkcnt_t)size && (max < 0 || max_space < space))
		{
			max_space=space;
			max=i;
		}
	}
	return max;
}

/* get diridx for maximum free space */
int get_free_dir(void)
{
	size_t i, max, max_perc, max_perc_space = 0;
	struct statvfs stf;
	fsblkcnt_t max_space = 0;
    fsblkcnt_t space = 0;
    fsblkcnt_t perclimit = 0;
    size_t perc = 0;

    if(mhdd->move_limit==0){
        return find_free_space(0);
    }

    for (max = i = 0; i < mhdd->dirs.size(); i++){
		if (statvfs(mhdd->dirs[i].c_str(), &stf) != 0)
			continue;
		space = stf.f_bsize;
		space *= stf.f_bavail;

		if (mhdd->move_limit <= 100) {

			if (mhdd->move_limit != 100) {
				perclimit = stf.f_blocks;

				if (mhdd->move_limit != 99) {
					perclimit *= mhdd->move_limit + 1;
					perclimit /= 100;
				}

				if (stf.f_bavail >= perclimit)
					return i;
			}

			perc = 100 * stf.f_bavail / stf.f_blocks;

			if (perc > max_perc_space) {
				max_perc_space = perc;
				max_perc = i;
			}
		} else {
			if (space >= (fsblkcnt_t)mhdd->move_limit)
				return i;
		}

		if(space > (fsblkcnt_t)max_space) {
			max_space = space;
			max = i;
		}
	}


	if (!max_space && !max_perc_space) {
		mhdd_debug(MHDD_INFO,
			"get_free_dir: Can't find freespace\n");
		return -1;
	}

	if (max_perc_space)
		return max_perc;
	return max;
}

static int reopen_files(struct openFile * file, const std::string new_name)
{
	int i=-1;
	int error = 0;
    off_t seek = 0;
	int flags = 0;
	int fh;
    struct openFile *next = NULL;

	mhdd_debug(MHDD_INFO, "reopen_files: %s -> %s\n",
			file->parent->real_name.c_str(), new_name.c_str());

    i=-1;
    for (auto& kv : file->parent->descriptors) {
        i++;

        next = kv.second;

	//for (i = 0; i<file->parent->descriptors.size(); i++) {
		//next = &file->parent->descriptors[i];

		seek = lseek(next->fh, 0, SEEK_CUR);
		flags = next->flags;

		flags &= ~(O_EXCL|O_TRUNC);

		/* open */
		if ((fh = open(new_name.c_str(), flags)) == -1) {
			mhdd_debug(MHDD_INFO,
				"reopen_files: error reopen: %s\n",
				strerror(errno));
			if (!i) {
				error = errno;
				break;
			}
			close(next->fh);
		}
		else
		{
			/* seek */
			if (seek != lseek(fh, seek, SEEK_SET)) {
				mhdd_debug(MHDD_INFO,
					"reopen_files: error seek %s\n",
					strerror(errno));
				close(fh);
				if (!i) {
					error = errno;
					break;
				}
			}

			/* filehandle */
			if (dup2(fh, next->fh) != next->fh) {
				mhdd_debug(MHDD_INFO,
					"reopen_files: error dup2 %s\n",
					strerror(errno));
				close(fh);
				if (!i) {
					error = errno;
					break;
				}
			}
			/* close temporary filehandle */
			mhdd_debug(MHDD_MSG,
				"reopen_files: reopened %s (to %s) old h=%x "
				"new h=%x seek=%lld\n",
				next->parent->real_name.c_str(), new_name.c_str(), next->fh, fh, seek);
			close(fh);
		}
	}

	if (error) {
		return -error;
	}

    file->parent->real_name = new_name;

	return 0;
}

int move_file(struct openFile * file, off_t wsize)
{
	std::string from, to;
	off_t size;

	int ret, dir_id;
	struct utimbuf ftime = {0};
	struct statvfs svf;
	fsblkcnt_t space;
	struct stat st;

	mhdd_debug(MHDD_MSG, "move_file: %s\n", file->parent->real_name.c_str());

	from = file->parent->real_name;

	/* We need to check if already moved */
	if (statvfs(from.c_str(), &svf) != 0)
		return -errno;
	space = svf.f_bsize;
	space *= svf.f_bavail;

	/* get file size */
	if (fstat(file->fh, &st) != 0) {
		mhdd_debug(MHDD_MSG, "move_file: error stat %s: %s\n",
			from.c_str(), strerror(errno));
		return -errno;
	}

        /* Hard link support is limited to a single device, and files with
           >1 hardlinks cannot be moved between devices since this would
           (a) result in partial files on the source device (b) not free
           the space from the source device during unlink. */
	if (st.st_nlink > 1) {
		mhdd_debug(MHDD_MSG, "move_file: cannot move "
			"files with >1 hardlinks\n");
		return -ENOTSUP;
	}

	size = st.st_size;
	if (size < wsize) size=wsize;

	if (space > (fsblkcnt_t)size) {
		mhdd_debug(MHDD_MSG, "move_file: we have enough space\n");
		return 0;
	}

	if ((dir_id=find_free_space(size)) == -1) {
		mhdd_debug(MHDD_MSG, "move_file: can not find space\n");
		return -1;
	}

	create_parent_dirs(dir_id, file->parent->name);

	to = create_path(mhdd->dirs[dir_id], file->parent->name);

	mhdd_debug(MHDD_MSG, "move_file: move %s to %s\n", from.c_str(), to.c_str());

	/* move data */
    try{
        std::ifstream source(from.c_str(), std::ios::binary);
        std::ofstream dest(to.c_str(), std::ios::binary);

        std::istreambuf_iterator<char> begin_source(source);
        std::istreambuf_iterator<char> end_source;
        std::ostreambuf_iterator<char> begin_dest(dest); 
        std::copy(begin_source, end_source, begin_dest);

        source.close();
        dest.close();
    }catch (const std::exception& ex) {
        unlink(to.c_str());
        return -errno;
    }catch(...){
        unlink(to.c_str());
        return -errno;
    }

	mhdd_debug(MHDD_MSG, "move_file: done move data\n");

    /* owner/group/permissions */
    chmod(to.c_str(), st.st_mode);
	chown(to.c_str(), st.st_uid, st.st_gid);

	/* time */
	ftime.actime = st.st_atime;
	ftime.modtime = st.st_mtime;
	utime(to.c_str(), &ftime);

#ifndef WITHOUT_XATTR
        /* extended attributes */
        if (copy_xattrs(from, to) == -1)
            mhdd_debug(MHDD_MSG,
                    "copy_xattrs: error copying xattrs from %s to %s\n",
                    from.c_str(), to.c_str());
#endif

	if ((ret = reopen_files(file, to)) == 0)
		unlink(from.c_str());
	else
		unlink(to.c_str());

	mhdd_debug(MHDD_MSG, "move_file: %s -> %s: done, code=%d\n",
		from.c_str(), to.c_str(), ret);

	return ret;
}

#ifndef WITHOUT_XATTR
int copy_xattrs(const std::string &from, const std::string &to)
{
        int listsize=0, attrvalsize=0;
        std::vector<char> listbuf, attrvalbuf;
        char *name_begin=NULL, *name_end=NULL;

        /* if not xattrs on source, then do nothing */
        if ((listsize=listxattr(from.c_str(), NULL, 0)) == 0)
                return 0;

        /* get all extended attributes */
        listbuf.resize(listsize);
        if (listxattr(from.c_str(), &listbuf[0], listsize) == -1)
        {
                mhdd_debug(MHDD_MSG,
                        "listxattr: error listing xattrs on %s : %s\n",
                        from.c_str(), strerror(errno));
                return -1;
        }

        /* loop through each xattr */
        for(name_begin=&listbuf[0], name_end=&listbuf[0]+1;
                name_end < (&listbuf[0] + listsize); name_end++)
        {
                /* skip the loop if we're not at the end of an attribute name */
                if (*name_end != '\0')
                        continue;

                /* get the size of the extended attribute */
                attrvalsize = getxattr(from.c_str(), name_begin, NULL, 0);
                if (attrvalsize < 0)
                {
                        mhdd_debug(MHDD_MSG,
                                "getxattr: error getting xattr size on %s name %s : %s\n",
                                from.c_str(), name_begin, strerror(errno));
                        return -1;
                }

                /* get the value of the extended attribute */
                attrvalbuf.resize(attrvalsize);
                if (getxattr(from.c_str(), name_begin, &attrvalbuf[0], attrvalsize) < 0)
                {
                        mhdd_debug(MHDD_MSG,
                                "getxattr: error getting xattr value on %s name %s : %s\n",
                                from.c_str(), name_begin, strerror(errno));
                        return -1;
                }

                /* set the value of the extended attribute on dest file */
                if (setxattr(to.c_str(), name_begin, &attrvalbuf[0], attrvalsize, 0) < 0)
                {
                        mhdd_debug(MHDD_MSG,
                                "setxattr: error setting xattr value on %s name %s : %s\n",
                                from.c_str(), name_begin, strerror(errno));
                        return -1;
                }

                /* point the pointer to the start of the attr name to the start */
                /* of the next attr */
                name_begin = name_end+1;
                name_end++;
        }

        return 0;
}
#endif

std::string create_path(const std::string &dir, const std::string &file)
{
    std::stringstream path;
    path << dir;
    if(dir.size() && dir[dir.size()-1]!='/'){ path << "/"; }
    path << file;

    std::string final = path.str();
    if(final.size() && final[final.size()-1]=='/'){ final.erase(final.size()-1); }

    return final;
}

std::string find_path(const std::string file)
{
	size_t i;
	struct stat st;

    for (i = 0; i < mhdd->dirs.size(); i++){
		std::string path = create_path(std::string(mhdd->dirs[i]), file);
		if (lstat(path.c_str(), &st)==0) return path;
	}
	return std::string();
}

int find_path_id(const std::string file)
{
	size_t i;
	struct stat st;

	for (i = 0; i < mhdd->dirs.size(); i++){
		std::string path=create_path(mhdd->dirs[i], file);
		if (lstat(path.c_str(), &st)==0)
		{
			return i;
		}
	}

	return -1;
}


int create_parent_dirs(int dir_id, const std::string &path)
{
    std::string parent, exists, path_parent;
	struct stat st;
    int res = 0;

	mhdd_debug(MHDD_DEBUG,
		"create_parent_dirs: dir_id=%d, path=%s\n", dir_id, path.c_str());
	parent = get_parent_path(path);
	if (parent.size()<=0) return 0;

	exists = find_path(parent);
	if (exists.size()<=0) { errno=EFAULT; return -errno; }

	path_parent=create_path(mhdd->dirs[dir_id], parent);

	/* already exists */
	if (stat(path_parent.c_str(), &st)==0)
	{
		return 0;
	}

	/* create parent dirs */
	res=create_parent_dirs(dir_id, parent);

	if (res!=0)
	{
		return res;
	}

	/* get stat from exists dir */
	if (stat(exists.c_str(), &st)!=0)
	{
		return -errno;
	}
	res=mkdir(path_parent.c_str(), st.st_mode);
	if (res==0)
	{
		chown(path_parent.c_str(), st.st_uid, st.st_gid);
		chmod(path_parent.c_str(), st.st_mode);
	}
	else
	{
		res=-errno;
		mhdd_debug(MHDD_DEBUG,
			"create_parent_dirs: can not create dir %s: %s\n",
			path_parent.c_str(),
			strerror(errno));
	}

#ifndef WITHOUT_XATTR
        /* copy extended attributes of parent dir */
        if (copy_xattrs(exists, path_parent) == -1)
            mhdd_debug(MHDD_MSG,
                    "copy_xattrs: error copying xattrs from %s to %s\n",
                    exists.c_str(), path_parent.c_str());
#endif

	return res;
}

std::string get_parent_path(std::string path)
{
	if (path.size()>0 && path[path.size()-1]=='/'){
        path.erase(path.size()-1);
    }
    size_t pos = path.find_last_of('/');
    if(pos==std::string::npos){
        pos = 0;
    }
    path.erase(pos);

    return path;
}

std::string get_base_name(std::string path)
{
    std::string file = NULL;
	int len = path.size();

	if (len && path[len-1]=='/'){
        path.erase(len-1);
    }

    size_t pos = path.find_last_of('/');
    if(pos!=std::string::npos){
        return path.substr(pos+1);
    }
    return path;
}

/* return true if directory is empty */
int dir_is_empty(const std::string &path)
{
	DIR * dir = opendir(path.c_str());
	struct dirent *de;

	if (!dir)
		return -1;
	while((de = readdir(dir))) {
		if (strcmp(de->d_name, ".") == 0) continue;
		if (strcmp(de->d_name, "..") == 0) continue;
		closedir(dir);
		return 0;
	}

	closedir(dir);
	return 1;
}

unsigned long
hash_str(const char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

std::string file_get_contents(const std::string path){
    std::ifstream t(path.c_str());
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}
