#ifndef SPRINGY_OPENFILES_HPP
#define SPRINGY_OPENFILES_HPP

#include <boost/filesystem.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include "volume/ivolume.hpp"

namespace Springy{
    class OpenFiles{
        public:
            struct openFile{
                boost::filesystem::path volumeFile;

                ::Springy::Volume::IVolume *volume;

                int fd; // file descriptor is unique
                int flags;
                mode_t mode;

                mutable int *syncToken;
            };

        protected:
                struct of_idx_volumeFile{};
                struct of_idx_fd{};
                struct openFileSetEntry{
                    OpenFiles::openFile o;

                    int fd;

                    mutable bool valid;
                    bool operator<(const openFileSetEntry& of)const{return o.fd < of.o.fd; }
                    bool isLocal()const{ return o.volume->isLocal(); }
                    boost::filesystem::path volumeFile()const{ return o.volumeFile; }
                };

                typedef boost::multi_index::multi_index_container<
                  openFileSetEntry,
                  boost::multi_index::indexed_by<
                    // sort by openFile::operator<
                    boost::multi_index::ordered_unique<boost::multi_index::tag<of_idx_fd>, boost::multi_index::member<openFileSetEntry,int,&openFileSetEntry::fd> >,

                    // sort by less<string> on name
                    boost::multi_index::ordered_non_unique<boost::multi_index::tag<of_idx_volumeFile>, boost::multi_index::const_mem_fun<openFileSetEntry,boost::filesystem::path,&openFileSetEntry::volumeFile> >,
                    boost::multi_index::ordered_non_unique<boost::multi_index::const_mem_fun<openFileSetEntry,bool,&openFileSetEntry::isLocal> >
                  > 
                > openFiles_set;

                openFiles_set openFiles;
        public:
            OpenFiles();
            ~OpenFiles();

            int add(boost::filesystem::path volumeFile, ::Springy::Volume::IVolume *volume, int internalFd, int flags, mode_t mode=0);
            openFile getByDescriptor(int fd);
            void remove(int fd);
    };
}

#endif /* OPENFILES_HPP */

