#include "../src/fuse.hpp"
#include "../src/settings.hpp"

#include "../src/libc/libc.hpp"
#include "../src/util/uri.hpp"

#include <set>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>

#include <boost/filesystem.hpp>

#define ASSERT(condition) { if(!(condition)){ std::cerr << "ASSERT FAILED: " << #condition << " @ " << __FILE__ << " (" << __LINE__ << "):" << __FUNCTION__ << " | errno=" << strerror(errno) << std::endl; } assert((condition)); }


class TestMoveFileLibC : public Springy::LibC::LibC{
    public:
        int writecount;
        virtual ssize_t pwrite(int LINE, int fd, const void *buf, size_t count, off_t offset){
            this->writecount++;
            if(this->writecount==1){
                errno = ENOSPC;
                return -1;
            }

            return Springy::LibC::LibC::pwrite(LINE, fd, buf, count, offset);
        }
};

char cwd[PATH_MAX];
std::unordered_map<std::string, struct stat> fillerMap;
TestMoveFileLibC *libc = new TestMoveFileLibC();

void test_getattr(Springy::Fuse &f){
    struct stat buf1, buf2;

    {
        std::string pathb = std::string(cwd)+"/dir1/b";
        ::stat(pathb.c_str(), &buf1);
        int rval = f.op_getattr("b", &buf2);
        ASSERT(rval==0);
        ASSERT(::memcmp(&buf1,&buf2,sizeof(buf1))==0);
    }

    {
        std::string pathb = std::string(cwd)+"/dir1/noexist";
        ::stat(pathb.c_str(), &buf1);
        int tmperrno = errno;

        int rval = f.op_getattr("noexist", &buf2);
        ASSERT(-rval == tmperrno);
    }
}

void test_statfs(Springy::Fuse &f){
    struct statvfs vfsassert, vfstest;

    {
        std::string pathb = std::string(cwd)+"/dir1/b";
        ::statvfs(pathb.c_str(), &vfsassert);
        int rval = f.op_statfs("b", &vfstest);
        ASSERT(rval==0);
        ASSERT(::memcmp(&vfsassert,&vfstest,sizeof(vfsassert))==0);
    }

    {
        std::string pathb = std::string(cwd)+"/dir1/noexist";
        errno = 0;
        ::statvfs(pathb.c_str(), &vfsassert);
        int tmperrno = errno;
        int rval = f.op_statfs("noexist", &vfstest);
        ASSERT(-rval == tmperrno);
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

        ASSERT(rval == 0);
        ASSERT(fillerMap.size() >= 5);

        ::stat((basepath+"/dir1/.").c_str(), &buf);
        ASSERT(::memcmp(&buf,&(fillerMap.find(".")->second),sizeof(buf))==0);
        ::stat((basepath+"/dir1/..").c_str(), &buf);
        ASSERT(::memcmp(&buf,&(fillerMap.find("..")->second),sizeof(buf))==0);

        ::stat((basepath+"/dir1/a").c_str(), &buf);
        ASSERT(::memcmp(&buf,&(fillerMap.find("a")->second),sizeof(buf))==0);
        ::stat((basepath+"/dir1/b").c_str(), &buf);
        ASSERT(::memcmp(&buf,&(fillerMap.find("b")->second),sizeof(buf))==0);
        ::stat((basepath+"/dir2/c").c_str(), &buf);
        ASSERT(::memcmp(&buf,&(fillerMap.find("c")->second),sizeof(buf))==0);
    }

    {
        fillerMap.clear();

        int rval = 0;
        rval = f.op_readdir("/noexist", NULL, test_fuse_filler_dir_t, 0, NULL);
        ASSERT(-rval == ENOENT);
        rval = f.op_readdir("/b", NULL, test_fuse_filler_dir_t, 0, NULL);
        ASSERT(-rval == ENOTDIR);
    }
}

void test_readlink(Springy::Fuse &f){
    {
        char result[8192];

        int rval = f.op_readlink("/c/testlink", result, sizeof(result));
        ASSERT(rval==0);
        ASSERT(std::string(result)=="../../test.fuse");
    }
    {
        char result[8192];

        int rval = f.op_readlink("/c/testlink", result, sizeof(result));
        ASSERT(rval==0);
        ASSERT(std::string(result)=="../../test.fuse");
    }
}


void test_access(Springy::Fuse &f){
    {
        int rval = f.op_access("/c/testlink", R_OK|W_OK);
        ASSERT(rval==0);
    }
    {
        int rval = f.op_access("/noexist", R_OK|W_OK);
        ASSERT(rval==-ENOENT);
        rval = f.op_access("/b", R_OK|W_OK|X_OK);
        ASSERT(rval==-EACCES);
    }
}

void test_mkdir_rmdir(Springy::Fuse &f){
    ::rmdir((std::string(cwd)+"/dir1/a/test").c_str());
    {
        int rval = f.op_mkdir("/a/test", 0666);
        ASSERT(rval==0);
    }
    {
        int rval = f.op_rmdir("/a/test");
        ASSERT(rval==0);
        rval = f.op_rmdir("/noexist");
        ASSERT(rval==-ENOENT);
    }
}

void test_unlink(Springy::Fuse &f){
    FILE *fp = fopen((std::string(cwd)+"/dir1/a/testfile").c_str(), "w+");
    if(fp){
        fclose(fp);
    }

    {
        int rval = f.op_unlink("/a/testfile");
        ASSERT(rval==0);
    }
    {
        int rval = f.op_unlink("/noexist");
        ASSERT(rval==-ENOENT);
    }
}

void test_truncate(Springy::Fuse &f){
    std::string abspath = std::string(cwd)+"/dir1/a/testfile";
    unlink(abspath.c_str());
    FILE *fp = fopen(abspath.c_str(), "w+");
    if(fp){
        fputs("test", fp);
        fclose(fp);
    }

    {
        int rval = f.op_truncate("/a/testfile", 1);
        ASSERT(rval == 0);
        struct stat buf;
        ASSERT(stat(abspath.c_str(), &buf) == 0);
        ASSERT(buf.st_size == 1);
    }
}

void test_rename(Springy::Fuse &f){
    std::string abspath = std::string(cwd)+"/dir1/a/testfile2";
    unlink(abspath.c_str());
    FILE *fp = fopen(abspath.c_str(), "w+");
    if(fp){
        fputs("test", fp);
        fclose(fp);
    }

    {
        int rval = f.op_rename("/a/testfile2", "/a/testfile3");
        ASSERT(rval == 0);
        struct stat buf;
        ASSERT(stat((std::string(cwd)+"/dir1/a/testfile3").c_str(), &buf) == 0);
    }
}

void test_utimens(Springy::Fuse &f){
    std::string abspath = std::string(cwd)+"/dir1/b";
    struct timespec tsOrig, ts[2];

    clock_gettime(CLOCK_REALTIME, &tsOrig);
    ts[0] = tsOrig;
    ts[1] = tsOrig;

    {
        int rval = f.op_utimens("/b", ts);
        ASSERT(rval == 0);
        struct stat st;
        stat(abspath.c_str(), &st);
        ASSERT(memcmp(&st.st_atim, &tsOrig, sizeof(struct timespec)) == 0);
        ASSERT(memcmp(&st.st_mtim, &tsOrig, sizeof(struct timespec)) == 0);
    }
}

void test_chmod(Springy::Fuse &f){
    std::string abspath = std::string(cwd)+"/dir1/b";
    FILE *fp = fopen(abspath.c_str(), "a+");
    if(fp){ fclose(fp); }
    mode_t m = S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH;
    chmod(abspath.c_str(), S_IRUSR|S_IRGRP|S_IROTH);

    mode_t mask = umask(0);
    umask(mask);

    {
        int rval = f.op_chmod("/b", m);
        ASSERT(rval == 0);
        struct stat st;
        stat(abspath.c_str(), &st);
        ASSERT(m == (st.st_mode & ~S_IFREG));
    }
}

void test_chown(Springy::Fuse &f){
    std::string abspath = std::string(cwd)+"/dir1/b";

    struct stat st;
    stat(abspath.c_str(), &st);

    {
        int rval = f.op_chown("/b", st.st_uid, st.st_gid);  // dont change anything
        ASSERT(rval == 0);
    }
}

void test_symlink(Springy::Fuse &f){
    std::string linkpath = std::string(cwd)+"/dir1/slink";

    unlink(linkpath.c_str());

    {
        int rval = f.op_symlink("/b", "/slink");
        ASSERT(rval == 0);
        struct stat st;
        ASSERT(lstat(linkpath.c_str(), &st) == 0);
    }
}

void test_link(Springy::Fuse &f){
    std::string linkpath = std::string(cwd)+"/dir1/hlink";
    unlink(linkpath.c_str());

    {
        int rval = f.op_link("b", "/hlink");
        ASSERT(rval == 0);
        struct stat st;
        ASSERT(lstat(linkpath.c_str(), &st) == 0);
    }
}

void test_mknod(Springy::Fuse &f){
    std::string nodpath = std::string(cwd)+"/dir1/nod";
    unlink(nodpath.c_str());

    {
        mode_t m = S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH;

        int rval = f.op_mknod("/nod", m, S_IFIFO);
        ASSERT(rval == 0);
        struct stat st;
        ASSERT(stat(nodpath.c_str(), &st) == 0);
    }
}

void test_xattr(Springy::Fuse &f){
    std::string xattrpath = std::string(cwd)+"/dir1/xattr";
    unlink(xattrpath.c_str());

    FILE *fp = fopen(xattrpath.c_str(), "a+");
    fclose(fp);

    {
        int rval = f.op_setxattr("/xattr", "user.test", "test", 4, XATTR_CREATE);
        ASSERT(rval == 0);
        char buffer[128];
        rval = f.op_getxattr("/xattr", "user.test", buffer, sizeof(buffer));
        ASSERT(rval == 4);
        ASSERT( std::string(buffer) == std::string("test") );
        
        memset(buffer, '\0', sizeof(buffer));

        rval = f.op_listxattr("/xattr", buffer, sizeof(buffer));
        ASSERT( std::string(buffer) == std::string("user.test") );
        ASSERT(rval == 10);
        rval = f.op_removexattr("/xattr", "user.test");
        ASSERT(rval == 0);
        rval = f.op_listxattr("/xattr", buffer, sizeof(buffer));
        ASSERT(rval == 0);
    }
}

void test_fops(Springy::Fuse &f){
    //test_create(const std::string file, mode_t mode, struct fuse_file_info *fi);
    //test_open(const std::string file, struct fuse_file_info *fi);
    //test_release(const std::string path, struct fuse_file_info *fi);

    //test_read(const std::string, char *buf, size_t count, off_t offset, struct fuse_file_info *fi);
    //test_write(const std::string file, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi);
    //test_ftruncate(const std::string path, off_t size, struct fuse_file_info *fi);
    //test_fsync(const std::string path, int isdatasync, struct fuse_file_info *fi);

    std::string fopspath = std::string(cwd)+"/dir1/fops";
    unlink(fopspath.c_str());

    struct fuse_file_info fi;

    {
        int rval = 0;
        rval = f.op_create("/fops", S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH, &fi);
        ASSERT(rval == 0);
        rval = f.op_release("/fops", &fi);
        ASSERT(rval == 0);
        fi.fh    = 0;
        fi.flags = O_RDWR | O_APPEND;
        rval = f.op_open("/fops", &fi);
        ASSERT(rval == 0);

        char buffer[128] = {'\0'};
        snprintf(buffer, sizeof(buffer)-1, "test");
        rval = f.op_write("/fops", buffer, 4, 0, &fi);

        ASSERT(rval == 4);
        rval = f.op_fsync("/fops", 1, &fi);
        ASSERT(rval == 0);
        memset(buffer, '\0', sizeof(buffer));
        rval = f.op_read("/fops", buffer, sizeof(buffer), 0, &fi);
        ASSERT(rval == 4);
        ASSERT(memcmp(buffer, "test", 4) == 0);
        rval = f.op_ftruncate("/fops", 0, &fi);
        ASSERT(rval == 0);
        struct stat st;
        stat(fopspath.c_str(), &st);
        ASSERT(st.st_size == 0);
        rval = f.op_release("/fops", &fi);
        ASSERT(rval == 0);
    }
}

void test_moveFile(Springy::Fuse &f){
    std::string movepath = std::string(cwd)+"/dir2/moveFile",
                movedpath = std::string(cwd)+"/dir1/moveFile";
    unlink(movepath.c_str());
    unlink(movedpath.c_str());

    struct fuse_file_info fi;

    {
        FILE *fp = fopen(movepath.c_str(), "w+");
        fwrite("toast", 5, 1, fp);
        fclose(fp);

        int rval = 0;
        fi.flags = O_RDWR;
        rval = f.op_open("/moveFile", &fi);
        ASSERT(rval == 0);

        libc->writecount = 0;
        rval = f.op_write("/moveFile", "test", 4, 0, &fi);
        ASSERT(rval == 4);

        struct stat st;
        rval = stat(movedpath.c_str(), &st);
        ASSERT(rval == 0);
        ASSERT(st.st_size == 5);
        rval = stat(movepath.c_str(), &st);
        ASSERT(rval == -1);
        rval = f.op_release("/moveFile", &fi);
        ASSERT(rval == 0);
    }
}

class TestFuse : public Springy::Fuse{
    protected:
        Springy::Settings s;

    public:
        TestFuse() : Springy::Fuse(){
            this->init(&this->s, ::libc);
        }
        void test_findRelevantPaths(){
            {
                // simple root directory
                this->config->directories.clear();
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/dir1"), boost::filesystem::path("/")));

                std::map<boost::filesystem::path, boost::filesystem::path> result;
                result = this->findRelevantPaths(boost::filesystem::path("/some/where"));
                ASSERT(result.size()==1);
                ASSERT(result.begin()->second.string()=="/");
                result = this->findRelevantPaths(boost::filesystem::path("/"));
                ASSERT(result.size()==1);
                ASSERT(result.begin()->second.string()=="/");
            }

            {
                // two simple root directory
                this->config->directories.clear();
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/dir1"), boost::filesystem::path("/")));
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/dir2"), boost::filesystem::path("/")));

                std::map<boost::filesystem::path, boost::filesystem::path> result;
                result = this->findRelevantPaths(boost::filesystem::path("/some/where"));
                ASSERT(result.size()==2);
                ASSERT(result.begin()->first == "/dir1");
                ASSERT((++result.begin())->first == "/dir2");

                result = this->findRelevantPaths(boost::filesystem::path("/"));
                ASSERT(result.size()==2);
                ASSERT(result.begin()->first == "/dir1");
                ASSERT((++result.begin())->first == "/dir2");
            }

            {
                // complicated directory structure with root
                this->config->directories.clear();
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/root1"), boost::filesystem::path("/")));
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/root2"), boost::filesystem::path("/")));
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/deep1"), boost::filesystem::path("/some/where/deep/in/the/directory/path")));
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/deep2"), boost::filesystem::path("/some/where/deep/in/the/directory/path")));
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/diff1"), boost::filesystem::path("/completely/different/path")));

                std::map<boost::filesystem::path, boost::filesystem::path> result;
                result = this->findRelevantPaths(boost::filesystem::path("/not/specified"));
                ASSERT(result.size()==2);
                ASSERT(result.begin()->first == "/root1");
                ASSERT((++result.begin())->first == "/root2");

                result = this->findRelevantPaths(boost::filesystem::path("/"));
                ASSERT(result.size()==2);
                ASSERT(result.begin()->first == "/root1");
                ASSERT((++result.begin())->first == "/root2");

                result = this->findRelevantPaths(boost::filesystem::path("/some/where/deep/in/the/directory/path/and/even/deeper"));
                ASSERT(result.size()==2);
                ASSERT(result.begin()->first == "/deep1");
                ASSERT((++result.begin())->first == "/deep2");

                result = this->findRelevantPaths(boost::filesystem::path("/completely/different/path/sub/directory"));
                ASSERT(result.size()==1);
                ASSERT(result.begin()->first == "/diff1");
            }

            {
                // complicated directory structure without root
                this->config->directories.clear();
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/deep1"), boost::filesystem::path("/some/where/deep/in/the/directory/path")));
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/deep2"), boost::filesystem::path("/some/where/deep/in/the/directory/path")));
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/diff1"), boost::filesystem::path("/completely/different/path")));

                std::map<boost::filesystem::path, boost::filesystem::path> result;
                result = this->findRelevantPaths(boost::filesystem::path("/not/specified"));
                ASSERT(result.size()==0);

                result = this->findRelevantPaths(boost::filesystem::path("/"));
                ASSERT(result.size()==0);

                result = this->findRelevantPaths(boost::filesystem::path("/some/where/deep"));
                ASSERT(result.size()==0);

                result = this->findRelevantPaths(boost::filesystem::path("/completely/different"));
                ASSERT(result.size()==0);

                result = this->findRelevantPaths(boost::filesystem::path("/some/where/deep/in/the/directory/path/and/even/deeper"));
                ASSERT(result.size()==2);
                ASSERT(result.begin()->first == "/deep1");
                ASSERT((++result.begin())->first == "/deep2");

                result = this->findRelevantPaths(boost::filesystem::path("/completely/different/path/sub/directory"));
                ASSERT(result.size()==1);
                ASSERT(result.begin()->first == "/diff1");
            }

            {
                // cascading directory structure
                this->config->directories.clear();
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/where1"), boost::filesystem::path("/some/where")));
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/where2"), boost::filesystem::path("/some/where/deep")));
                this->config->directories.insert(std::make_pair(boost::filesystem::path("/where3"), boost::filesystem::path("/some/where/deep/in/the/directory/path")));

                std::map<boost::filesystem::path, boost::filesystem::path> result;
                result = this->findRelevantPaths(boost::filesystem::path("/not/specified"));
                ASSERT(result.size()==0);

                result = this->findRelevantPaths(boost::filesystem::path("/"));
                ASSERT(result.size()==0);

                result = this->findRelevantPaths(boost::filesystem::path("/some"));
                ASSERT(result.size()==0);

                result = this->findRelevantPaths(boost::filesystem::path("/completely/different"));
                ASSERT(result.size()==0);

                result = this->findRelevantPaths(boost::filesystem::path("/some/where"));
                ASSERT(result.size()==1);
                ASSERT(result.begin()->first == "/where1");

                result = this->findRelevantPaths(boost::filesystem::path("/some/where/deep"));
                ASSERT(result.size()==1);
                ASSERT(result.begin()->first == "/where2");

                result = this->findRelevantPaths(boost::filesystem::path("/some/where/deep/in/the/directory"));
                ASSERT(result.size()==1);
                ASSERT(result.begin()->first == "/where2");

                result = this->findRelevantPaths(boost::filesystem::path("/some/where/deep/in/the/directory/path/and/even/deeper"));
                ASSERT(result.size()==1);
                ASSERT(result.begin()->first == "/where3");
            }
        }
};

void test_Uri(){
    {
        Springy::Util::Uri u("http://user:pwd@host:8080/some/path?query=string&#fragment");
        //std::cout << u.protocol() << " | " << u.username() << " | "
        //          << u.password() << " | " << u.host() << " | "
        //          << u.port() << " | " << u.path() << " | "
        //          << u.query() << " | " << u.fragment() << std::endl;
        ASSERT(u.protocol() == "http");
        ASSERT(u.username() == "user");
        ASSERT(u.password() == "pwd");
        ASSERT(u.host() == "host");
        ASSERT(u.port() == 8080);
        ASSERT(u.path() == "/some/path");
        ASSERT(u.query() == "query=string&");
        ASSERT(u.fragment() == "fragment");
    }
    {
        // standard example
        Springy::Util::Uri u("http://host/some/path");
        ASSERT(u.protocol() == "http");
        ASSERT(u.username() == "");
        ASSERT(u.password() == "");
        ASSERT(u.host() == "host");
        ASSERT(u.port() == 0);
        ASSERT(u.path() == "/some/path");
        ASSERT(u.query() == "");
        ASSERT(u.fragment() == "");
    }
    {
        // ftp example
        Springy::Util::Uri u("ftp://host/some/path");
        ASSERT(u.protocol() == "ftp");
        ASSERT(u.username() == "");
        ASSERT(u.password() == "");
        ASSERT(u.host() == "host");
        ASSERT(u.port() == 0);
        ASSERT(u.path() == "/some/path");
        ASSERT(u.query() == "");
        ASSERT(u.fragment() == "");
    }
    {
        // file example
        Springy::Util::Uri u("file:///some/path/file.log");
        ASSERT(u.protocol() == "file");
        ASSERT(u.username() == "");
        ASSERT(u.password() == "");
        ASSERT(u.host() == "");
        ASSERT(u.host() == "");
        ASSERT(u.port() == 0);
        ASSERT(u.path() == "/some/path/file.log");
        ASSERT(u.query() == "");
        ASSERT(u.fragment() == "");
    }
    {
        Springy::Util::Uri u("/some/path/file.log");
        ASSERT(u.protocol() == "");
        ASSERT(u.username() == "");
        ASSERT(u.password() == "");
        ASSERT(u.host() == "");
        ASSERT(u.port() == 0);
        ASSERT(u.path() == "/some/path/file.log");
        ASSERT(u.query() == "");
        ASSERT(u.fragment() == "");
    }
}

int main(int argc, char **argv){
    getcwd(cwd, sizeof(cwd));

    libc->writecount=1;

    Springy::Settings s;
    s.directories.insert(std::make_pair(boost::filesystem::path(std::string(cwd)+"/dir1"), boost::filesystem::path("/")));  // virtualmount is /
    s.directories.insert(std::make_pair(boost::filesystem::path(std::string(cwd)+"/dir2"), boost::filesystem::path("/")));  // virtualmount is /

    Springy::Fuse f;
    f.init(&s, libc);

    test_getattr(f);

    test_statfs(f);
    test_readdir(f);
    test_readlink(f);
    
    test_fops(f);
    test_moveFile(f);

    test_truncate(f);
    test_access(f);
    test_mkdir_rmdir(f);
    test_unlink(f);

    test_rename(f);
    test_utimens(f);
    test_chmod(f);
    test_chown(f);
    test_symlink(f);
    test_link(f);
    test_mknod(f);

    test_xattr(f);

    TestFuse tf;
    tf.test_findRelevantPaths();
    
    test_Uri();

    return 0;
}
