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
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stddef.h>
#include <fuse.h>
#include <pthread.h>
#include <sys/mount.h>

#include <iostream>
#include <algorithm>

#include "parse_options.h"
#include "version.h"
#include "debug.h"
#include "tools.h"

#include "lock.hpp"

mhdd_config *mhdd = NULL;

#define MHDDFS_OPT(t, p, v) { t, offsetof(struct mhdd_config, p), v }
#define MHDD_VERSION_OPT 15121974
#define MHDD_FOREGROUND_OPT 15121975
#define MHDD_SINGLETHREADED_OPT 15121976
#define MHDD_DEBUGGING_OPT 15121977

/* the number less (or equal) than 100 is in percent,
   more than 100 is in bytes */
#define DEFAULT_MLIMIT ( 4l * 1024 * 1024 * 1024 )
#define MINIMUM_MLIMIT ( 50l * 1024 * 1024 )

static pthread_rwlock_t optionslock;

static struct fuse_opt mhddfs_opts[]={
	MHDDFS_OPT("mlimit=%s",   mlimit_str, 0),
	MHDDFS_OPT("logfile=%s",  debug_file, 0),
	MHDDFS_OPT("loglevel=%d", loglevel,   0),
    MHDDFS_OPT("port=%d", server_port,   0),
    MHDDFS_OPT("iface=%s", server_iface,   0),

    MHDDFS_OPT("keypem=%s", server_key_pem,   0),
    MHDDFS_OPT("certpem=%s", server_cert_pem,   0),

    MHDDFS_OPT("realm=%s", server_auth_realm,   0),
    MHDDFS_OPT("username=%s", server_auth_username,   0),
    MHDDFS_OPT("password=%s", server_auth_password,   0),
    MHDDFS_OPT("htdigest=%s", server_auth_htdigest,   0),

    FUSE_OPT_KEY("-f",        MHDD_FOREGROUND_OPT),
    FUSE_OPT_KEY("-s",        MHDD_SINGLETHREADED_OPT),
    FUSE_OPT_KEY("-s",        MHDD_DEBUGGING_OPT),

	FUSE_OPT_KEY("-V",        MHDD_VERSION_OPT),
	FUSE_OPT_KEY("--version", MHDD_VERSION_OPT),

	FUSE_OPT_END
};

void add_mhdd_dirs(const std::string dir)
{
    if(dir.size() == 0){ return; }
    std::cerr << __FILE__ << ":" << __LINE__ << std::endl;

	std::string add_dir(dir);

	if (add_dir[0]!='/'){
        std::vector<char> cpwd;
        cpwd.resize(PATH_MAX);

        while(getcwd(&cpwd[0], cpwd.size())==NULL){
            cpwd.resize(cpwd.size()*2);
        }

		add_dir = create_path(&cpwd[0], dir);
	}
    std::cerr << __FILE__ << ":" << __LINE__ << std::endl;

    if(mhdd->mount.size() && add_dir.find(mhdd->mount)!=std::string::npos){
        errno = EINVAL;
        mhdd_debug(MHDD_MSG, "add_mhdd_dirs: given directory '%s' is subdirectory of mountpoint '%s'\n", add_dir.c_str(), mhdd->mount.c_str());

        return;
    }
    std::cerr << __FILE__ << ":" << __LINE__ << std::endl;

    Lock optionsLck(optionslock);
    optionsLck.wrlock();

    mhdd->dirs.push_back(add_dir);
    std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
}
void rem_mhdd_dirs(const std::string dir){
    if(mhdd->dirs.size() <= 0){ return; }

    Lock optionsLck(optionslock);
    optionsLck.wrlock();

    mhdd->dirs.erase(std::remove(mhdd->dirs.begin(), mhdd->dirs.end(), dir), mhdd->dirs.end());
}

int mhddfs_opt_proc(void *data,
		const char *arg, int key, struct fuse_args *outargs)
{
    printf("argument parameter: %s\n", arg);
	switch(key)
	{
		case MHDD_VERSION_OPT:
			fprintf(stderr, "mhddfs version: %s\n", VERSION);
			exit(0);

        case MHDD_FOREGROUND_OPT:
            return 1;
        case MHDD_SINGLETHREADED_OPT:
            return 1;
        case MHDD_DEBUGGING_OPT:
            return 1;

		case FUSE_OPT_KEY_NONOPT:
			{
                std::cerr << __FILE__ << ":" << __LINE__ << std::endl;

                std::string dir(arg);
                add_mhdd_dirs(dir);
                std::cerr << __FILE__ << ":" << __LINE__ << std::endl;

				return 0;
			}
	}
	return 1;
}

static void check_if_unique_mountpoints(void)
{
	size_t i, j;
    std::vector<struct stat> stats;
    stats.resize(mhdd->dirs.size());

	for (i = 0; i < mhdd->dirs.size(); i++) {
		if (stat(mhdd->dirs[i].c_str(), &stats[i]) != 0)
			memset(&stats[i], 0, sizeof(struct stat));

		for (j = 0; j < i; j++) {
			if (mhdd->dirs[i].find(mhdd->mount)!=std::string::npos && mhdd->dirs[i]!=mhdd->dirs[j]) {
				/*  mountdir isn't unique */
				if (stats[j].st_dev != stats[i].st_dev)
					continue;
				if (stats[j].st_ino != stats[i].st_ino)
					continue;
				if (!stats[i].st_dev)
					continue;
				if (!stats[i].st_ino)
					continue;
			}

			fprintf(stderr,
				"mhddfs: Duplicate directories: %s %s\n"
				"\t%s was excluded from dirlist\n",
				mhdd->dirs[i].c_str(),
				mhdd->dirs[j].c_str(),
				mhdd->dirs[i].c_str()
			);

            mhdd->dirs.erase(mhdd->dirs.begin()+i);

			i--;
			break;
		}
	}
}


