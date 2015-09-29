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
#include <pthread.h>

#include <unordered_map>

#include "flist.h"
#include "debug.h"
#include "tools.h"

#include "lock.hpp"

static pthread_rwlock_t descslock;
std::unordered_map<std::string, struct openFiles*> descs;

/* init */
void flist_init(void)
{
    descs.clear();
    pthread_rwlock_init(&descslock, 0);
	/* pthread_rwlock_init(&files_lock, 0); */
}

/* add file to list */
struct openFile * flist_create(const std::string name,
		const std::string real_name, int flags, int fh)
{
    Lock descl(descslock);

    std::unordered_map<std::string, struct openFiles*>::iterator it;

    try{
        if(name.size()<=0 || real_name.size()<=0){
            errno = EINVAL;
            return NULL;
        }

        descl.rdlock();

        it = descs.find(name);
        if(it == descs.end()){
            descl.wrlock();

            it = descs.find(name);
            if(it == descs.end()){
                descs.insert(std::make_pair(name, new openFiles()));
            }

            struct openFiles *entry = descs[name];

            entry->name = name;
            entry->real_name = real_name;
            entry->descriptors.clear();

            pthread_rwlock_init(&entry->lock, 0);

            it = descs.find(name);
        }

        Lock entryl(it->second->lock);
        entryl.wrlock();

        {
            // here it's assumed that the entry never changes and thus the strings wont change
            struct openFile *add = new struct openFile;
            add->flags = flags;
            add->fh = fh;
            add->parent = it->second;
            it->second->descriptors.insert(std::make_pair(fh, add));
        }

        return it->second->descriptors[fh];
    } catch (const std::exception& ex) {
    } catch (...) {
    }

	return NULL;
}

/* internal function */
void flist_delete(struct openFile *item)
{
    if(item==NULL){
        errno = EINVAL;
        return;
    }

    struct openFiles *parent = item->parent;

    Lock descl(descslock);
    descl.rdlock();

    {
        Lock entryl(parent->lock);
        entryl.wrlock();

        item->parent->descriptors.erase(item->fh);
        delete item;
        item = NULL;
    }

    {
        descl.wrlock();

        Lock entryl(parent->lock);
        entryl.wrlock();

        if(parent->descriptors.size()<=0){
            entryl.unlock();
            descs.erase(parent->name);
            parent = NULL;
        }

        delete parent;
    }
}
