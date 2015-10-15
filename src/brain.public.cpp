#include "brain.hpp"

#include "util/string.hpp"
#include "util/file.hpp"

#include <sys/mount.h>
#include <sys/resource.h>
#include <string.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include <boost/algorithm/string.hpp>

namespace Springy{
    boost::optional<int> Brain::exitStatus;

    Brain& Brain::instance(){
        static Brain applicationInstance;
        return applicationInstance;
    }

    Brain::~Brain(){}

    Brain& Brain::init(){
        boost::log::core::get()->set_filter
        (
            boost::log::trivial::severity >= boost::log::trivial::info
        );

        // Declare the supported options.
        this->visibleDesc.add_options()
            ("help", "show help")
            ("option,o", po::value<std::vector<std::string> >()->composing(), "additional options like id, logfile, loglevel, fuse options")
            ("debug,d", "additional debugging output")
            ("foreground,f", "run in foreground")
    #ifdef HAS_FUSE
            ("single,s", "run fuse single threaded")
    #endif
        ;
        this->hiddenDesc.add_options()
            ("input", po::value<std::vector<std::string> >()->composing(), "Input")
        ;
        this->m_positional.add("input", -1);
        
        struct rlimit limit;
	    limit.rlim_cur = 512000;
	    limit.rlim_max = 512000;

	    if(setrlimit(RLIMIT_NOFILE, &limit) != 0)
	    {
		   this->exitStatus = -1;
		   std::cerr << "setrlimit failed";
		   return *this;
	    }

        try{
            this->fuse.init(&this->config);
        }catch(std::runtime_error &e){
            std::cerr << e.what() << std::endl;
            this->exitStatus = -1;
            return *this;
        }
        
        try{
            httpd.init(&this->config);
        }catch(std::runtime_error &e){
            std::cerr << e.what() << std::endl;
            this->exitStatus = -1;
            return *this;
        }

        return *this;
    }
    Brain& Brain::setUp(int argc, char *argv[]){
        try{
            po::variables_map vm;
            po::options_description allDesc;
            allDesc.add(this->hiddenDesc).add(this->visibleDesc);

            po::command_line_parser parser = po::command_line_parser(argc, argv);
            po::parsed_options parsed = parser.options(allDesc)
                                              .positional(this->m_positional)
                                              .allow_unregistered()
                                              //.style(po::command_line_style::unix_style)
                                              .run();
            po::store(parsed, vm);
            po::notify(vm);

            if (vm.count("help")) {
                this->printHelp(std::cout);
                this->exitStatus = boost::optional<int>(0);
                return *this;
            }
            

            std::vector<std::string> opts;
            if(vm.count("input")){
                opts = vm["input"].as<std::vector<std::string> >();
            }

            std::string mountpoint;
            std::set<std::string> directories;

            switch(opts.size()){
                case 0:
                    throw std::runtime_error("there must be exactly one mount point and zero or more volume directories");
                default:
                    {
                        mountpoint = opts[opts.size()-1];
                        switch(::access(mountpoint.c_str(), F_OK)){
                            case -1:
                                if(errno == ENOTCONN){
                                    std::string fuserumountCommand = std::string("fusermount -u \"")+mountpoint+"\"";
                                    system(fuserumountCommand.c_str());
                                    if(::access(mountpoint.c_str(), F_OK)==0){
                                        break;
                                    }

                                    if(::umount(mountpoint.c_str())!=0){
                                        std::cerr << strerror(errno) << std::endl;
                                        throw std::runtime_error(std::string("couldnt unmount given mountpoint: ")+mountpoint);
                                    }
                                    if(::access(mountpoint.c_str(), F_OK)!=0){
                                        throw std::runtime_error(std::string("inaccessable mount point given: ")+mountpoint);
                                    }
                                }
                                throw std::runtime_error(std::string("inaccessable mount point given: ")+mountpoint);
                        }

                        mountpoint = Util::File::realpath(mountpoint);
                        for(unsigned int i=0;i<opts.size()-1;i++){
                            std::string file = opts[i];
                            std::vector<std::string> tmpdirs;
                            try{
                                file = Util::File::realpath(file);
                                tmpdirs.push_back(file);
                            }catch(...){
                                boost::split( tmpdirs, opts[0], boost::is_any_of(","), boost::token_compress_on );
                            }

                            for(unsigned int i=0;i<tmpdirs.size();i++){
                                std::string directory = Util::File::realpath(Util::String::urldecode(tmpdirs[i]));
                                
                                if(directory.find(mountpoint)!=std::string::npos){
                                    throw std::runtime_error(std::string("directory within mountpoint not allowed: ")+directory);
                                }

                                directories.insert(directory);
                            }
                        }
                    }
                    break;
                }

                for (const auto& dir: directories) {
                    struct stat info;
                    if (stat(dir.c_str(), &info))
                    {
                        throw std::runtime_error(std::string("cannot stat: ")+dir);
                    }
                    if (!S_ISDIR(info.st_mode))
                    {
                        throw std::runtime_error(std::string("not a directory: ")+dir);
                    }
                }

                this->config.option("mountpoint", mountpoint);
                this->config.option("directories", directories);

                std::set<std::string> options;
                std::vector<std::string> cmdoptions;
                if(vm.count("option")){
                    cmdoptions = vm["option"].as<std::vector<std::string> >();
                }

                for(unsigned int i=0;i<cmdoptions.size();i++){
                    std::vector<std::string> current;
                    boost::split( current, cmdoptions[i], boost::is_any_of(","), boost::token_compress_on );
                    std::copy( cmdoptions.begin(), cmdoptions.end(), std::inserter( options, options.end() ) );
                }

                this->config.option("options", options);

                if (vm.count("foreground")) {
                    this->config.option("foreground", true);
                }
                else{
                    this->config.option("foreground", false);
                }

                this->fuse.setUp(vm.count("single")!=0);
        }catch(std::runtime_error &e){ 
          std::cerr << "ERROR: " << e.what() << std::endl << std::endl; 
          this->printHelp(std::cerr);
          this->exitStatus = -1;
          return *this;
        }catch(po::error &e){ 
          std::cerr << "ERROR: " << e.what() << std::endl << std::endl; 
          this->printHelp(std::cerr);
          this->exitStatus = -1;
          return *this;
        } 
    /*
        size_t i=0;
        struct fuse_args tmp = FUSE_ARGS_INIT(argc, argv);

        pthread_rwlock_init(&optionslock, 0);

        //mhdd->args = (struct fuse_args*)calloc(1, sizeof(struct fuse_args));
        mhdd->server_port = 4100;
        mhdd->server_key_pem = NULL;
        mhdd->server_cert_pem = NULL;

        mhdd->server_auth_realm = NULL;
        mhdd->server_auth_username = NULL;
        mhdd->server_auth_password = NULL;
        mhdd->server_auth_htdigest = NULL;

        //if(mhdd->server_cert_pem!=NULL && mhdd->server_key_pem!=NULL){
        //    mhdd->server_cert_pem = file_get_contents(mhdd->server_cert_pem);
        //    mhdd->server_key_pem = file_get_contents(mhdd->server_key_pem);
        //    if(mhdd->server_cert_pem!=NULL || mhdd->server_key_pem!=NULL){
        //        if(mhdd->server_cert_pem!=NULL){ free(mhdd->server_cert_pem); }
        //        else{
        //            fprintf(stderr,
        //                    "mhddfs: '%s' - invalid cert file\n\n",
        //                    mhdd->mount);
        //        }
        //        if(mhdd->server_key_pem!=NULL){ free(mhdd->server_key_pem); }
        //        else{
        //            fprintf(stderr,
        //                    "mhddfs: '%s' - invalid key file\n\n",
        //                    mhdd->mount);
        //        }
        //        exit(-1);
        //    }
        //}

        if(mhdd->server_auth_htdigest!=NULL && access( mhdd->server_auth_htdigest, F_OK ) == -1){
                fprintf(stderr,
                        "mhddfs: '%s' - htdigest given but doesnt exist (%s)\n\n",
                        mhdd->mount.c_str(), mhdd->server_auth_htdigest);
                exit(-1);
        }
        */
        return *this;
    }
    void Brain::signalHandler(boost::system::error_code error, int signal_number){
        switch(signal_number){
            case SIGHUP:
                break;
            case SIGINT: case SIGTERM:
                Brain::instance().io_service.stop();
                break;
        }
    }
    Brain& Brain::run(){
        if(this->exitStatus){
            return *this;
        }
        
        this->httpd.start();

        if(false==this->config.option<bool>("foreground")){
            pid_t process_id = fork();
            if(process_id<0){
                // fork failed
                this->exitStatus = -1;
                return *this;
            }
            else if(process_id > 0){
                exit(0);
            }

            // child process

            //unmask the file mode
            umask(0);
            //set new session
            setsid();
            // Change the current working directory to root.
            chdir("/");
            // Close stdin. stdout and stderr
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }

        signals.async_wait(Brain::signalHandler);

        io_service.run();

        /*
            memset (&act, '\0', sizeof(act));
            act.sa_sigaction = &mhdd_termination_handler;
            // The SA_SIGINFO flag tells sigaction() to use the sa_sigaction field, not sa_handler.
            act.sa_flags = SA_SIGINFO;
            if (sigaction(SIGINT, &act, NULL) < 0 ||
                sigaction(SIGHUP, &act, NULL) < 0 ||
                sigaction(SIGTERM, &act, NULL) < 0
               ){
                 fprintf(stderr, "set signal handler failed\n");
                 mhddfs_httpd_stopServer();
                Brain::exitStatus = -1;
                break;
            }
        */

        return *this;
    }
    Brain& Brain::tearDown(){
        if(this->exitStatus){
            return *this;
        }

        this->httpd.stop();
        this->fuse.tearDown();

        return *this;
    }
}
