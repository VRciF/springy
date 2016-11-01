#ifndef __CLOSEONDESTRUCT_HPP__
#define __CLOSEONDESTRUCT_HPP__

#include <unistd.h>
#include <fcntl.h>

template<typename T>
class CloseOnDestruct{
    protected:
        T t;
        bool enabled;

    public:
        CloseOnDestruct(){ this->enabled = false; }
        CloseOnDestruct(T t){ this->t = t; this->enabled = true; }
        CloseOnDestruct(const CloseOnDestruct &cod){
            this->enabled = true;
            this->t = cod.t;
        }

        void enable(){ this->enabled = true; }
        void disable(){ this->enabled = false; }

        
        ~CloseOnDestruct(){
            if(!this->enabled){ return; }

            fcntl(this->t, F_GETFD);
            if(errno != EBADF){
                close(this->t);
            }
        }

        CloseOnDestruct& operator= (const T& x) { this->t = x; return *this;}
        CloseOnDestruct& operator= (const CloseOnDestruct& cod) {
            this->t = cod.t;
            this->enabled = true;
            return *this;
        }
        operator T() { return this->t; }
};

#endif
