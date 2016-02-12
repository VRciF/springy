# example compile without fuse: make WITHOUT_FUSE=1

SRC := $(shell find src -name '*.cpp')
OBJ := $(patsubst src/%.cpp,obj/%.o,$(SRC))
CPPFLAGS := -ggdb3 -std=c++11 -Isrc -Wall -pedantic -MMD -DBOOST_ALL_DYN_LINK
LDFLAGS := -pthread -lboost_log -lboost_program_options -lboost_thread -lboost_system -lboost_filesystem

ifndef WITHOUT_FUSE
    CPPFLAGS := $(CPPFLAGS) -DHAS_FUSE $(shell pkg-config fuse --cflags) -DFUSE_USE_VERSION=29
    LDFLAGS := $(LDFLAGS) $(shell pkg-config fuse --libs)
endif

TARGET := springy

$(TARGET): $(OBJ)
	$(CXX) $(CPPFLAGS) src/fossa.c $(OBJ) $(LDFLAGS) -o $@

obj/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) -nostdlib $(CXXFLAGS) -o $@ -c $<

clean:
	rm obj/*.o
