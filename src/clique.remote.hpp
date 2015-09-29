#ifndef __CLIQUEREMOTE_HPP__
#define __CLIQUEREMOTE_HPP__

#include "kraken.hpp"

class CliqueRemote : public Kraken::ISubscriber, public IClique{
    public:
        CliqueRemote(std::string ns){
            this->ns = ns;
        }

        virtual void OnPeerUp(const Kraken::peer &p){
            if(this->ns!=p.ns){ return; }
        }
        virtual void OnPeerDown(const Kraken::peer &p){
            if(this->ns!=p.ns){ return; }
        }

        bool isRemote(){ return true; }
        bool isLocal(){ return false; }

    protected:
        std::string ns;
};

#endif
