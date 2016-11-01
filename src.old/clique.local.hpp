#ifndef __CLIQUELOCAL_HPP__
#define __CLIQUELOCAL_HPP__

#include "iclique.hpp"

#include <string>

class CliqueLocal : public IClique{
    public:
        CliqueLocal(std::string ns){
            this->ns = ns;
        }

        virtual void OnDirectoryUp(const std::string directory){
        }
        virtual void OnDirectoryDown(const std::string directory){
        }

        bool isRemote(){ return false; }
        bool isLocal(){ return true; }

    protected:
        std::string ns;
};

#endif
