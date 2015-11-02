#ifndef __BRAIN_HPP__
#define __BRAIN_HPP__

#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.
#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>

#include <boost/optional.hpp>

#include <iostream>
#include <fuse.h>

#include "fuse.hpp"
#include "httpd.hpp"
#include "libc/ilibc.hpp"
#include "settings.hpp"

namespace po = boost::program_options;

namespace Springy{
    class Brain{
        public:
            static boost::optional<int> exitStatus;

            static Brain& instance();
            ~Brain();

            Brain& init();
            Brain& setUp(int argc, char *argv[]);
            Brain& run();
            Brain& tearDown();

            static void signalHandler(boost::system::error_code error, int signal_number);
            void printHelp(std::ostream & output);

        protected:
            boost::asio::io_service io_service;
            boost::asio::io_service::work preventIOServiceFromExitWorker;

            boost::uuids::uuid instanceUUID;
            boost::asio::signal_set signals;

            po::positional_options_description m_positional;
            po::options_description visibleDesc;
            po::options_description hiddenDesc;

            Springy::LibC::ILibC *libc;
            Settings config;
            Fuse fuse;
            Httpd httpd;

            bool showusage;
            

            Brain();
    };
}
#endif
