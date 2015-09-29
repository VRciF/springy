#ifndef __LOCK_HPP__

class Lock{
    protected:
        pthread_rwlock_t &lock;
        bool locked;

    public:
        Lock(pthread_rwlock_t &lock) : lock(lock){
            locked = false;
        }

        void rdlock(){
            if(locked){
                pthread_rwlock_unlock(&lock);
            }
            pthread_rwlock_rdlock(&lock);
            this->locked = true;
        }
        void wrlock(){
            if(locked){
                pthread_rwlock_unlock(&lock);
            }
            pthread_rwlock_wrlock(&lock);
            this->locked = true;
        }
        void unlock(){
            pthread_rwlock_unlock(&lock);
            this->locked = false;
        }
        
        ~Lock(){
            if(locked){
                this->unlock();
            }
        }
};

#endif
