#include "kraken.hpp"

void* Kraken::execTask(void *arg){
    static int execRunning = false;

    if(execRunning == true){ return NULL; }

    Kraken *instance = static_cast<Kraken *>(arg);
    execRunning = true;
    instance->run();
    execRunning = false;
    return NULL;
}

Kraken::Kraken(std::string ns, std::string identifier, int port=4101){
    this->port = port;
    this->ns = ns;
    this->identifier = identifier;

    this->heartbeatIntervalInSeconds = 60;

    this->socksrv_inaddr_anyv4 = -1;
    this->socksrv_inaddr_anyv6 = -1;
    this->socksrv_mcastv4 = -1;
    this->socksrv_mcastv6 = -1;

    this->v4McastIp = std::string("239.255.146.253");
    this->v6McastIp = std::string("FF02:0:0:0:0:0:0:1");

    this->keeprunning = true;
    this->isrunning = false;
}
Kraken::~Kraken(){
    this->keeprunning = false;
    pthread_join(this->th, NULL);
}

Kraken& Kraken::start(){
    pthread_create(&this->th, NULL, Kraken::execTask, (void*) this);
    return *this;
}

int Kraken::getPort(){ return this->port; }
Kraken& Kraken::setPort(int port){
    this->port = port;
    return *this;
}

void Kraken::subscribe(Kraken::ISubscriber *ref){
    this->subscribers.insert(ref);
}
void Kraken::unsubscribe(Kraken::ISubscriber *ref){
    this->subscribers.erase(ref);
}
