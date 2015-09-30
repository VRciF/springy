#include "brain.hpp"

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

    /*
            mhdd = new mhdd_config;

            struct sigaction act;
            int multithreaded=0;
            char *mountpoint=NULL;
            int res;

            fops.getattr    	= mhdd_stat;
            fops.statfs     	= mhdd_statfs;
            fops.readdir    	= mhdd_readdir;
            fops.readlink   	= mhdd_readlink;
            fops.open       	= mhdd_fileopen;
            fops.release    	= mhdd_release;
            fops.read       	= mhdd_read;
            fops.write      	= mhdd_write;
            fops.create     	= mhdd_create;
            fops.truncate   	= mhdd_truncate;
            fops.ftruncate  	= mhdd_ftruncate;
            fops.access     	= mhdd_access;
            fops.mkdir      	= mhdd_mkdir;
            fops.rmdir      	= mhdd_rmdir;
            fops.unlink     	= mhdd_unlink;
            fops.rename     	= mhdd_rename;
            fops.utimens    	= mhdd_utimens;
            fops.chmod      	= mhdd_chmod;
            fops.chown      	= mhdd_chown;
            fops.symlink    	= mhdd_symlink;
            fops.mknod      	= mhdd_mknod;
            fops.fsync      	= mhdd_fsync;
            fops.link		    = mhdd_link;
        #ifndef WITHOUT_XATTR
                fops.setxattr   	= mhdd_setxattr;
                fops.getxattr   	= mhdd_getxattr;
                fops.listxattr  	= mhdd_listxattr;
                fops.removexattr	= mhdd_removexattr;
        #endif
            fops.init		    = mhdd_init;
            fops.destroy		= mhdd_destroy;

            mhdd_debug_init();
    */
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

            std::vector<std::string> opts = vm["input"].as<std::vector<std::string> >();
            if(opts.size()!=0){
                if(opts.size()!=2){
                    throw std::runtime_error("there must be either no mount points and volume directories OR both");
                }
                else{
                    std::vector<std::string> directories;
                    boost::split( directories, opts[0], boost::is_any_of(","), boost::token_compress_on );
                    std::string mountpoint = opts[1];
                    
                    std::cout << "mountpoint: " << mountpoint << std::endl;
                    for(unsigned int i=0;i<directories.size();i++){
                        std::cout << "directory: " << directories[i] << std::endl;
                    }
                }
            }
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
        if (vm.count("compression")) {
            std::cout << "Compression level was set to " 
         << vm["compression"].as<int>() << ".\n";
        } else {
            std::cout << "Compression level was not set.\n";
        }
    */
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

        mhdd->loglevel=MHDD_DEFAULT_DEBUG_LEVEL;

        {
            memcpy(&mhdd->args, &tmp, sizeof(struct fuse_args));
        }

        if (fuse_opt_parse(&mhdd->args, &mhdd, mhddfs_opts, mhddfs_opt_proc)==-1)
            usage(stderr);

        if (mhdd->dirs.size()<1) usage(stderr);

        mhdd->mount = mhdd->dirs[mhdd->dirs.size()-1];
        mhdd->dirs.resize(mhdd->dirs.size()-1);

        char *mount = realpath(mhdd->mount.c_str(), NULL);
        if(mount==NULL){
            fprintf(stderr,
                "mhddfs: '%s' - invalid mountpoint (%d:%s)\n\n",
                mhdd->mount.c_str(), errno, strerror(errno));
            exit(-1);
        }
        mhdd->mount = std::string(mount);
        free(mount);

        for(i=0;i<mhdd->dirs.size();i++){
            if(mhdd->dirs[i].find(mhdd->mount)!=std::string::npos){
                fprintf(stderr,
                    "mhddfs: '%s' - invalid directory, within mountpoint '%s'\n\n",
                    mhdd->dirs[i].c_str(),
                    mhdd->mount.c_str());
                exit(-1);
            }
        }

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

        check_if_unique_mountpoints();

        #if FUSE_VERSION >= 27
        mhdd->FUSE_MP_OPT_STR.append("-osubtype=mhddfs,fsname=");
        #else
        mhdd->FUSE_MP_OPT_STR.append("-ofsname=mhddfs#");
        #endif

        for (i=0; i<mhdd->dirs.size(); i++)
        {
            if (i) mhdd->FUSE_MP_OPT_STR.append(";");
            mhdd->FUSE_MP_OPT_STR.append(mhdd->dirs[i]);
        }
        if(!i){
            mhdd->FUSE_MP_OPT_STR.append("none");
        }
        fuse_opt_insert_arg(&mhdd->args, 1, mhdd->FUSE_MP_OPT_STR.c_str());
        fuse_opt_insert_arg(&mhdd->args, 1, mhdd->mount.c_str());

        for(i=0; i<mhdd->dirs.size(); i++)
        {
            struct stat info;
            if (stat(mhdd->dirs[i].c_str(), &info))
            {
                fprintf(stderr,
                    "mhddfs: can not stat '%s': %s\n",
                    mhdd->dirs[i].c_str(), strerror(errno));
                exit(-1);
            }
            if (!S_ISDIR(info.st_mode))
            {
                fprintf(stderr,
                    "mhddfs: '%s' - is not directory\n\n",
                    mhdd->dirs[i].c_str());
                exit(-1);
            }

            fprintf(stderr,
                "mhddfs: directory '%s' added to list\n",
                mhdd->dirs[i].c_str());
        }

        do{
            if(access(mhdd->mount.c_str(), F_OK)==-1){
                if(errno == ENOTCONN){
                    if(umount(mhdd->mount.c_str())==0 && access(mhdd->mount.c_str(), F_OK)==0){
                        break;
                    }
                }
                fprintf(stderr,
                    "mhddfs: inaccessable mount point given '%s' (%d:%s)\n",
                    mhdd->mount.c_str(), errno, strerror(errno));
            }
        }while(0);

        fprintf(stderr, "mhddfs: mount to: %s\n", mhdd->mount.c_str());
        fprintf(stderr, "mhddfs: loglevel: %d\n", mhdd->loglevel);

        if (mhdd->debug_file)
        {
            fprintf(stderr, "mhddfs: using debug file: %s, loglevel=%d\n",
                    mhdd->debug_file, mhdd->loglevel);
            mhdd->debug=fopen(mhdd->debug_file, "a");
            if (!mhdd->debug)
            {
                fprintf(stderr, "Can not open file '%s': %s",
                        mhdd->debug_file,
                        strerror(errno));
                exit(-1);
            }
            setvbuf(mhdd->debug, NULL, _IONBF, 0);
        }
        else{
            mhdd->debug_file = NULL;
            mhdd->debug = stderr;
        }

        //mhdd->move_limit = DEFAULT_MLIMIT;
        mhdd->move_limit = 0;

        if (mhdd->mlimit_str)
        {
            int len = strlen(mhdd->mlimit_str);

            if (len) {
                switch(mhdd->mlimit_str[len-1])
                {
                    case 'm':
                    case 'M':
                        mhdd->mlimit_str[len-1]=0;
                        mhdd->move_limit=atoll(mhdd->mlimit_str);
                        mhdd->move_limit*=1024*1024;
                        break;
                    case 'g':
                    case 'G':
                        mhdd->mlimit_str[len-1]=0;
                        mhdd->move_limit=atoll(mhdd->mlimit_str);
                        mhdd->move_limit*=1024*1024*1024;
                        break;

                    case 'k':
                    case 'K':
                        mhdd->mlimit_str[len-1]=0;
                        mhdd->move_limit=atoll(mhdd->mlimit_str);
                        mhdd->move_limit*=1024;
                        break;

                    case '%':
                        mhdd->mlimit_str[len-1]=0;
                        mhdd->move_limit=atoll(mhdd->mlimit_str);
                        break;

                    default:
                        mhdd->move_limit=atoll(mhdd->mlimit_str);
                        break;
                }
            }

            if (mhdd->move_limit < MINIMUM_MLIMIT) {
                if (!mhdd->move_limit) {
                    mhdd->move_limit = DEFAULT_MLIMIT;
                } else {
                    if (mhdd->move_limit > 100)
                        mhdd->move_limit = MINIMUM_MLIMIT;
                }
            }
        }
        if (mhdd->move_limit <= 100)
            fprintf(stderr, "mhddfs: move size limit %ld%%\n", mhdd->move_limit);
        else
            fprintf(stderr, "mhddfs: move size limit %ld bytes\n", mhdd->move_limit);

        mhdd_debug(MHDD_MSG, " >>>>> mhdd " VERSION " started <<<<<\n");
        */
        return *this;
    }
    void Brain::signalHandler(boost::system::error_code error, int signal_number){}
    Brain& Brain::run(){
        if(this->exitStatus){
            return *this;
        }

        signals.async_wait(Brain::signalHandler);

        io_service.run();

        /*
        do{
            flist_init();

            if(!mhddfs_httpd_startServer()){
                fprintf(stderr, "failed to start webserver on port %d\n", mhdd->server_port);
                Brain::exitStatus = -1;
                break;
            }

            signal(SIGPIPE, SIG_IGN);

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

            //return fuse_main(args->argc, args->argv, &mhdd_oper, 0);
            mhdd->fuse = fuse_setup(mhdd->args.argc, mhdd->args.argv, &mhdd_oper, sizeof(mhdd_oper),
                              &mountpoint, &multithreaded, &mhdd);

            if(mhdd->fuse==NULL){
                Brain::exitStatus = -1;
                break;
            }

            if (multithreaded){
                res = fuse_loop_mt(mhdd->fuse);
            }else{
                res = fuse_loop(mhdd->fuse);
            }
        }while(0);
        */

        return *this;
    }
    Brain& Brain::tearDown(){
        //fuse_teardown(mhdd->fuse, mountpoint);
        //delete mhdd;

        return *this;
    }
}
