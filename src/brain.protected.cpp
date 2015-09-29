#include "brain.hpp"

#include <fuse.h>
#include <stdio.h>


Brain::Brain() : preventIOServiceFromExitWorker(io_service),
                 instanceUUID(boost::uuids::random_generator()()),
                 signals(io_service, SIGHUP),
                 desc("Springy options")
{}

struct fuse_operations& Brain::getFuseOperations(){
    static struct fuse_operations fops;
    return fops;
}

void Brain::usage()
{
    FILE * to = stdout;
	const char *usage=
		"\n"
		"Multi-hdd FUSE filesystem\n"
		" Copyright (C) 2008, Dmitry E. Oboukhov <dimka@avanto.org>\n"
		"\n"
		"Usage:\n"
		" mhddfs dir1,dir2.. mountpoint [ -o OPTIONS ]\n"
		"\n"
		"OPTIONS:\n"
		"  mlimit=xxx - limit of the disk free space (if the disk\n"
		"          has the free space more than specified - it is\n"
		"          considered as the empty one).  Default is  4Gb,\n"
		"          but 100Mb at least.\n"
		"  logfile=/path/to/file  -  path to a file where the logs\n"
		"          will be stored.\n"
		"  loglevel=x - level for log-messages:\n"
		"                0 - debug\n"
		"                1 - info\n"
		"                2 - default messages\n"
        "  port=4100  -  http server port\n"
        "  iface=0.0.0.0  -  interface ip or name to bind http server to, default all interfaces (0.0.0.0)\n"
        "  keypem=/path/to/key.pem  -  path to SSL key file\n"
        "                              can be created with: openssl genrsa -out server.key.pem 4096\n"
        "  certpem=/path/to/cert.pem  -  path to SSL cert file\n"
        "                                can be created with: openssl req -days 365 -out server.cert.pem -new -x509 -key server.key.pem\n"
        "  realm=xxx  -  http server basic/digest authentication realm\n"
        "  username=xxx  -  http server authentication username\n"
        "  password=xxx  -  http server plain text password\n"
        "  digest=xxx  -  http server digest password authentication\n"
        "  htdigest=/path/to/.htdigest  -  http server file to apache formated .htdigest file\n"
		"\n"
		" see fusermount(1) for information about other options\n"
		"";

    fputs(usage, to);

    if (to==stdout) Brain::exitStatus = 0;
    else Brain::exitStatus = -1;
/*
    delete mhdd;

	if (to==stdout) exit(0);
	exit(-1);
*/
}
