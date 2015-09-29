#ifndef __SPRINGY_FUSE_HPP__
#define __SPRINGY_FUSE_HPP__

#ifdef HAS_FUSE

#include <fuse.h>

namespace Springy{

class Fuse{
    public:
    protected:
        struct fuse_operations fops;
};

}


#endif

#endif
