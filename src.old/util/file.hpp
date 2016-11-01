#ifndef SPRINGY_UTIL_FILE
#define SPRINGY_UTIL_FILE

#include "exception.hpp"

namespace Springy{
    namespace Util{
        class File{
            public:
                static std::string realpath(std::string file){
                    char *absolutepath = ::realpath(file.c_str(), NULL);
                    if(absolutepath == NULL){
                        throw Springy::Exception(__FILE__, __PRETTY_FUNCTION__, __LINE__) << "file doesnt exist: " << file;
                    }
                    file = std::string(absolutepath);
                    ::free(absolutepath);

                    return file;
                }
        };
    }
}

#endif
