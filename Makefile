# example compile without fuse: make WITHOUT_FUSE=1

SRC := $(shell find src -name '*.cpp')
OBJ := $(patsubst src/%.cpp,obj/%.o,$(SRC))
CPPFLAGS := -ggdb3 -std=c++11 -Isrc -Wall -pedantic -MMD -DBOOST_ALL_DYN_LINK
LDFLAGS := -rdynamic -pthread -lboost_log -lboost_program_options -lboost_thread -lboost_system -lboost_filesystem -lulockmgr

ifndef WITHOUT_FUSE
    CPPFLAGS := $(CPPFLAGS) -DHAS_FUSE $(shell pkg-config fuse --cflags) -DFUSE_USE_VERSION=29
    LDFLAGS := $(LDFLAGS) $(shell pkg-config fuse --libs)
endif

TARGET := springy

$(TARGET): setup $(OBJ) obj/mongose.o
	$(CXX) $(CPPFLAGS) obj/mongoose.o $(OBJ) $(LDFLAGS) -o $@

obj/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) -nostdlib $(CXXFLAGS) -o $@ -c $<

obj/mongose.o:
	$(CC) -o obj/mongoose.o -c src/mongoose.c

setup:
	mkdir -p obj
	mkdir -p obj/volume
	mkdir -p obj/fsops

clean:
	find obj -name "*.o" -exec rm {} \;
	find obj -name "*.d" -exec rm {} \;
