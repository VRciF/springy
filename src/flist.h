#ifndef __FLIST_H__
#define __FLIST_H__

#include <pthread.h>
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

typedef struct openFile{
    int fh;
    int flags;

    struct openFiles *parent;
} openFile;

typedef struct openFiles
{
	std::string name;
	std::string real_name;

    std::unordered_map<int, struct openFile*> descriptors;

    pthread_rwlock_t lock;
} openFiles;

typedef union map_fhOpenFile{
    uint64_t fh;
    struct openFile *file;
} map_fhOpenFile;


void flist_init(void);

struct openFile * flist_create(const std::string name,
		const std::string real_name, int flags, int fh);
void flist_delete(struct openFile *item);

#endif
