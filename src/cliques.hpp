#ifndef __CLIQUES_HPP__
#define __CLIQUES_HPP__

#include "clique.local.hpp"
#include "clique.remote.hpp"

#include <vector>

class Cliques{
    public:
        CliqueLocal& getCliqueLocal();

    protected:
        CliqueLocal cliquelocal;
        std::vector<CliqueRemote> remotes;
};

#endif
