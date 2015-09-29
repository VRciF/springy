#ifndef __BRAIN_HPP__
#define __BRAIN_HPP__

#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.
#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>

namespace po = boost::program_options;

class Brain{
    public:
        static int exitStatus;
        
        static Brain& instance();
        ~Brain();

        Brain& init();
        Brain& setUp(int argc, char *argv[]);
        Brain& run();
        Brain& tearDown();
        
        static void handler(boost::asio::signal_set& this_, boost::system::error_code error, int signal_number){
            
        }

    protected:
        boost::asio::io_service io_service;
        boost::asio::io_service::work preventIOServiceFromExitWorker;

        boost::uuids::uuid instanceUUID;
        boost::asio::signal_set signals;
        po::options_description desc;

        bool showusage;

        Brain();
        
        struct fuse_operations& getFuseOperations();
        void usage();
};

#endif
