#ifndef SPRINGY_VOLUME_IVOLUME
#define SPRINGY_VOLUME_IVOLUME

namespace Springy{
    namespace Volume{
        class IVolume{
            public:
                virtual int lstat(boost::filesystem::path v_file_name, struct ::stat *buf) = 0;
                virtual int statvfs(boost::filesystem::path v_path, struct ::statvfs *stat) = 0;
        };
    }
}

#endif
