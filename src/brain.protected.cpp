#include "brain.hpp"

#include <stdio.h>

namespace Springy{
    Brain::Brain() : preventIOServiceFromExitWorker(io_service),
                     instanceUUID(boost::uuids::random_generator()()),
                     signals(io_service, SIGINT, SIGTERM, SIGHUP),
                     visibleDesc("Springy options")
    {}

    void Brain::printHelp(std::ostream & output){
        output << "Usage: springy [DIR1,DIR2,... MOUNTPOINT] [OPTIONS]" << std::endl;
        output << visibleDesc << std::endl;
        output << "FUSE options:" << std::endl
               << "-o allow_other         allow access to other users" << std::endl
               << "-o allow_root          allow access to root" << std::endl
               << "-o nonempty            allow mounts over non-empty file/dir" << std::endl
               << "-o default_permissions enable permission checking by kernel" << std::endl
               << "-o fsname=NAME         set filesystem name" << std::endl
               << "-o subtype=NAME        set filesystem type" << std::endl
               << "-o large_read          issue large read requests (2.4 only)" << std::endl
               << "-o max_read=N          set maximum size of read requests" << std::endl

               << "-o hard_remove         immediate removal (don't hide files)" << std::endl
               << "-o use_ino             let filesystem set inode numbers" << std::endl
               << "-o readdir_ino         try to fill in d_ino in readdir" << std::endl
               << "-o direct_io           use direct I/O" << std::endl
               << "-o kernel_cache        cache files in kernel" << std::endl
               << "-o [no]auto_cache      enable caching based on modification times (off)" << std::endl
               << "-o umask=M             set file permissions (octal)" << std::endl
               << "-o uid=N               set file owner" << std::endl
               << "-o gid=N               set file group" << std::endl
               << "-o entry_timeout=T     cache timeout for names (1.0s)" << std::endl
               << "-o negative_timeout=T  cache timeout for deleted names (0.0s)" << std::endl
               << "-o attr_timeout=T      cache timeout for attributes (1.0s)" << std::endl
               << "-o ac_attr_timeout=T   auto cache timeout for attributes (attr_timeout)" << std::endl
               << "-o intr                allow requests to be interrupted" << std::endl
               << "-o intr_signal=NUM     signal to send on interrupt (10)" << std::endl
               << "-o modules=M1[:M2...]  names of modules to push onto filesystem stack" << std::endl

               << "-o max_write=N         set maximum size of write requests" << std::endl
               << "-o max_readahead=N     set maximum readahead" << std::endl
               << "-o async_read          perform reads asynchronously (default)" << std::endl
               << "-o sync_read           perform reads synchronously" << std::endl
               << "-o atomic_o_trunc      enable atomic open+truncate support" << std::endl
               << "-o big_writes          enable larger than 4kB writes" << std::endl
               << "-o no_remote_lock      disable remote file locking" << std::endl;
    }
}
