#include "../src/fuse.hpp"
#include "../src/settings.hpp"

#include <set>
#include <unistd.h>
#include <linux/limits.h>

char cwd[PATH_MAX];

void test_getattr(Springy::Fuse &f){
    struct stat buf1, buf2;
    std::string pathb(cwd);
    pathb += "/dir1/b";
    //std::cout << pathb << std::endl;
    stat(pathb.c_str(), &buf1);
    int rval = f.op_getattr("b", &buf2);
    assert(rval==0);
    assert(memcmp(&buf1,&buf2,sizeof(buf1))==0);
}

int main(int argc, char **argv){
    getcwd(cwd, sizeof(cwd));

    std::set<std::string> directories;
    directories.insert(std::string(cwd)+"/dir1");
    directories.insert(std::string(cwd)+"/dir2");

    Springy::Settings s;
    s.option("directories", directories);

    Springy::Fuse f;
    f.init(&s);
    
    test_getattr(f);

    return 0;
}