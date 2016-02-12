#ifndef SPRINGY_VOLUME_FILE
#define SPRINGY_VOLUME_FILE

#include "ivolume.hpp"
#include "../libc/ilibc.hpp"

namespace Springy{
    namespace Volume{
        class File : public Springy::Volume::IVolume{
            protected:
                Springy::LibC::ILibC *libc;
                Springy::Util::Uri &u;

            public:
                File(Springy::LibC::ILibC *libc, Springy::Util::Uri &u);
                ~File();

                virtual int lstat(boost::filesystem::path v_file_name, struct stat *buf);
                virtual int statvfs(boost::filesystem::path v_path, struct ::statvfs *stat);
        };
    }
}

#endif
