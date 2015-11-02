#include "../src/fuse.hpp"
#include "../src/settings.hpp"

#include "../src/libc/libc.hpp"

#include <set>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>

std::set<std::string> directories;
char cwd[PATH_MAX];
std::unordered_map<std::string, struct stat> fillerMap;

void test_getattr(Springy::Fuse &f){
    struct stat buf1, buf2;

    {
        std::string pathb = std::string(cwd)+"/dir1/b";
        ::stat(pathb.c_str(), &buf1);
        int rval = f.op_getattr("b", &buf2);
        assert(rval==0);
        assert(::memcmp(&buf1,&buf2,sizeof(buf1))==0);
    }

    {
        std::string pathb = std::string(cwd)+"/dir1/noexist";
        ::stat(pathb.c_str(), &buf1);
        int tmperrno = errno;

        int rval = f.op_getattr("noexist", &buf2);
        assert(-rval == tmperrno);
    }
}

void test_statfs(Springy::Fuse &f){
    struct statvfs vfsassert, vfstest;

    {
        std::string pathb = std::string(cwd)+"/dir1/b";
        ::statvfs(pathb.c_str(), &vfsassert);
        int rval = f.op_statfs("b", &vfstest);
        assert(rval==0);
        assert(::memcmp(&vfsassert,&vfstest,sizeof(vfsassert))==0);
    }

    {
        std::string pathb = std::string(cwd)+"/dir1/noexist";
        errno = 0;
        ::statvfs(pathb.c_str(), &vfsassert);
        int tmperrno = errno;
        int rval = f.op_statfs("noexist", &vfstest);
        assert(-rval == tmperrno);
    }
}

int test_fuse_filler_dir_t(void *buf, const char *name, const struct stat *stbuf, off_t off){
    fillerMap.insert(std::make_pair(std::string(name), *stbuf));
    return 0;
}

void test_readdir(Springy::Fuse &f){
    {
        std::string basepath(cwd);

        struct stat buf;

        fillerMap.clear();
        int rval = f.op_readdir("/", NULL, test_fuse_filler_dir_t, 0, NULL);

        assert(rval == 0);
        assert(fillerMap.size() == 5);

        ::stat((basepath+"/dir1/.").c_str(), &buf);
        assert(::memcmp(&buf,&(fillerMap.find(".")->second),sizeof(buf))==0);
        ::stat((basepath+"/dir1/..").c_str(), &buf);
        assert(::memcmp(&buf,&(fillerMap.find("..")->second),sizeof(buf))==0);

        ::stat((basepath+"/dir1/a").c_str(), &buf);
        assert(::memcmp(&buf,&(fillerMap.find("a")->second),sizeof(buf))==0);
        ::stat((basepath+"/dir1/b").c_str(), &buf);
        assert(::memcmp(&buf,&(fillerMap.find("b")->second),sizeof(buf))==0);
        ::stat((basepath+"/dir2/c").c_str(), &buf);
        assert(::memcmp(&buf,&(fillerMap.find("c")->second),sizeof(buf))==0);
    }

    {
        fillerMap.clear();

        int rval = 0;
        rval = f.op_readdir("/noexist", NULL, test_fuse_filler_dir_t, 0, NULL);
        assert(-rval == ENOENT);
        rval = f.op_readdir("/b", NULL, test_fuse_filler_dir_t, 0, NULL);
        assert(-rval == ENOTDIR);
    }
}

void test_readlink(Springy::Fuse &f){
    {
        char result[8192];

        int rval = f.op_readlink("/c/testlink", result, sizeof(result));
        assert(rval==0);
        assert(std::string(result)=="../../test.fuse");
    }
    {
        char result[8192];

        int rval = f.op_readlink("/c/testlink", result, sizeof(result));
        assert(rval==0);
        assert(std::string(result)=="../../test.fuse");
    }
}


void test_access(Springy::Fuse &f){
    {
        int rval = f.op_access("/c/testlink", R_OK|W_OK);
        assert(rval==0);
    }
    {
        int rval = f.op_access("/noexist", R_OK|W_OK);
        assert(rval==-ENOENT);
        rval = f.op_access("/b", R_OK|W_OK|X_OK);
        assert(rval==-EACCES);
    }
}

void test_mkdir_rmdir(Springy::Fuse &f){
    ::rmdir((std::string(cwd)+"/dir1/a/test").c_str());
    {
        int rval = f.op_mkdir("/a/test", 0666);
        assert(rval==0);
    }
    {
        int rval = f.op_rmdir("/a/test");
        assert(rval==0);
        rval = f.op_rmdir("/noexist");
        assert(rval==-ENOENT);
    }
}

void test_unlink(Springy::Fuse &f){
    FILE *fp = fopen((std::string(cwd)+"/dir1/a/testfile").c_str(), "w+");
    if(fp){
        fclose(fp);
    }

    {
        int rval = f.op_unlink("/a/testfile");
        assert(rval==0);
    }
    {
        int rval = f.op_unlink("/noexist");
        assert(rval==-ENOENT);
    }
}

int main(int argc, char **argv){
    getcwd(cwd, sizeof(cwd));

    directories.insert(std::string(cwd)+"/dir1");
    directories.insert(std::string(cwd)+"/dir2");

    Springy::LibC::LibC libc;

    Springy::Settings s;
    s.option("directories", directories);

    Springy::Fuse f;
    f.init(&s, &libc);

    test_getattr(f);
    test_statfs(f);
    test_readdir(f);
    test_readlink(f);

    //test_create(const std::string file, mode_t mode, struct fuse_file_info *fi);
    //test_open(const std::string file, struct fuse_file_info *fi);
    //test_release(const std::string path, struct fuse_file_info *fi);
    //test_read(const std::string, char *buf, size_t count, off_t offset, struct fuse_file_info *fi);
    //test_write(const std::string file, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi);

    //test_truncate(const std::string path, off_t size);
    //test_ftruncate(const std::string path, off_t size, struct fuse_file_info *fi);

    test_access(f);
    test_mkdir_rmdir(f);
    test_unlink(f);

    //test_rename(const std::string from, const std::string to);
    //test_utimens(const std::string path, const struct timespec ts[2]);
    //test_chmod(const std::string path, mode_t mode);
    //test_chown(const std::string path, uid_t uid, gid_t gid);
    //test_symlink(const std::string from, const std::string to);
    //test_link(const std::string from, const std::string to);
    //test_mknod(const std::string path, mode_t mode, dev_t rdev);
    //test_fsync(const std::string path, int isdatasync, struct fuse_file_info *fi);

    //test_setxattr(const std::string file_name, const std::string attrname,
    //              const char *attrval, size_t attrvalsize, int flags);
    //test_getxattr(const std::string file_name, const std::string attrname, char *buf, size_t count);
    //test_listxattr(const std::string file_name, char *buf, size_t count);
    //test_removexattr(const std::string file_name, const std::string attrname);

    //assert(1==0);

    return 0;
}
