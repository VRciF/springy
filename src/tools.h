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
#ifndef __TOOLS__H__
#define __TOOLS__H__

#include <stdint.h>
#include <pthread.h>

#include <setjmp.h>

#define TRY_UTHASH do{ if( !setjmp(uthash_descs_buf__) ){
#define CATCH_UTHASH } else {
#define ETRY_UTHASH } }while(0)
#define THROW_UTHASH longjmp(uthash_descs_buf__, 1)

#undef uthash_fatal
#define uthash_fatal(msg) longjmp(uthash_descs_buf__, 1);

extern jmp_buf uthash_descs_buf__;

#include "flist.h"

int get_free_dir(void);
std::string create_path(const std::string &dir, const std::string &file);
std::string find_path(const std::string file);
int find_path_id(const std::string file);

int create_parent_dirs(int dir_id, const std::string &path);
#ifndef WITHOUT_XATTR
int copy_xattrs(const std::string &from, const std::string &to);
#endif

/* true if success */
int move_file(struct openFile * file, off_t size);


/* paths */
std::string get_parent_path(std::string path);
std::string get_base_name(std::string path);


/* others */
int dir_is_empty(const std::string &path);

unsigned long hash_str(const std::string str);
std::string file_get_contents(const std::string path);

//char* strautocat(char **buffer, const char *str1);

#define MOVE_BLOCK_SIZE     32768

#endif
