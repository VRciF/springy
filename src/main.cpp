#include "brain.hpp"

int main(int argc, char **argv){
    {
        Springy::Brain::instance().init().setUp(argc, argv).run().tearDown();
    }

    if(Springy::Brain::exitStatus){
        return *Springy::Brain::exitStatus;
    }
    return 0;
}
