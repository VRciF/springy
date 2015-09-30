#include "brain.hpp"

#include <stdio.h>

namespace Springy{
    Brain::Brain() : preventIOServiceFromExitWorker(io_service),
                     instanceUUID(boost::uuids::random_generator()()),
                     signals(io_service, SIGHUP),
                     visibleDesc("Springy options")
    {}

    struct ::fuse_operations& Brain::getFuseOperations(){
        static struct ::fuse_operations fops;
        return fops;
    }

    void Brain::printHelp(std::ostream & output){
        std::cout << "Usage: springy [DIR1,DIR2,... MOUNTPOINT] [OPTIONS]" << std::endl;
        std::cout << visibleDesc << std::endl;
    }
}
