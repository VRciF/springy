#ifndef SPRINGY_UTIL_FILE
#define SPRINGY_UTIL_FILE

namespace Springy{
    namespace Util{
        class File{
            public:
                static std::string realpath(std::string file){
                    char *absolutepath = ::realpath(file.c_str(), NULL);
                    if(absolutepath == NULL){
                        throw std::runtime_error(std::string("file doesnt exist: ")+file);
                    }
                    file = std::string(absolutepath);
                    ::free(absolutepath);

                    return file;
                }
        };
    }
}

#endif
