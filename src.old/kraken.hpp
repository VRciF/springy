#ifndef __KRAKEN_HPP__
#define __KRAKEN_HPP__

#include <pthread.h>
#include <sys/socket.h>

#include <vector>
#include <string>
#include <map>
#include <set>

#include "util/closeondestruct.hpp"

class Kraken{
    public:
        class peer{
            public:
                struct sockaddr_storage addr;

                long int lastHearbeatInMs;

                std::string ns;
                std::string identifier;
        };
        class ISubscriber{
            public:
                virtual void OnPeerUp(const Kraken::peer &p) = 0;
                virtual void OnPeerDown(const Kraken::peer &p) = 0;
        };

        Kraken(std::string ns, std::string identifier, int port);
        ~Kraken();

        Kraken& start();

        int getPort();
        Kraken& setPort(int port);

        static void* execTask(void *arg);

        void subscribe(Kraken::ISubscriber *ref);
        void unsubscribe(Kraken::ISubscriber *ref);

    protected:
        int heartbeatIntervalInSeconds;

        std::string ns;
        std::string identifier;
        int port;
        std::string v4McastIp;
        std::string v6McastIp;

        CloseOnDestruct<int> socksrv_inaddr_anyv4, socksrv_inaddr_anyv6, socksrv_mcastv4, socksrv_mcastv6;

        bool keeprunning;
        bool isrunning;
        pthread_t th;

        std::map<std::string, peer> peers;
        std::set<Kraken::ISubscriber *> subscribers;

        void run();

        void parseHeartbeat(std::string message, struct sockaddr_storage &remote);
        void broadcastHeartbeat(std::string message);
        void multicastHeartbeat(std::string message);
};

#endif
