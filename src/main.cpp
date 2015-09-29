#include "brain.hpp"

int main(int argc, char **argv){
    {
        Brain b;
        b.init().setUp(argc, argv).run().tearDown();
    }

    return Brain::exitStatus;
}
