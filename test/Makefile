
fuse:
	g++ -ggdb3 -std=c++11 -DHAS_FUSE $(shell pkg-config fuse --cflags) -DFUSE_USE_VERSION=29 -o test.fuse ../src/settings.cpp ../src/fuse.cpp test.fuse.cpp -lpthread $(shell pkg-config fuse --libs)
