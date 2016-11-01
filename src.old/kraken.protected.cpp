#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define SOCKET int

#ifdef __MINGW32__ 
#undef SOCKET
#undef socklen_t 
#define WINVER 0x0501 
#include <ws2tcpip.h> 
#define EWOULDBLOCK WSAEWOULDBLOCK
#define close closesocket
#define socklen_t int
typedef unsigned int in_addr_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#endif

#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>

/* Define IPV6_ADD_MEMBERSHIP for FreeBSD and Mac OS X */
#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif

#include <sstream>

#include "kraken.hpp"

void Kraken::run(){
    struct timeval lastHearbeat;
    lastHearbeat.tv_usec = lastHearbeat.tv_sec = 0;

    this->isrunning = true;
    while(this->keeprunning){
        struct timeval now;
        gettimeofday(&now, NULL);
        long int nowms = now.tv_sec * 1000 + now.tv_usec / 1000;

        // send heartbeat
        do{
            if(lastHearbeat.tv_sec + this->heartbeatIntervalInSeconds > now.tv_sec){
                break;
            }

            std::stringstream heartbeatMessage;
            heartbeatMessage << this->port << ":" << this->ns << ":" << this->identifier;

            this->broadcastHeartbeat(heartbeatMessage.str());
            this->multicastHeartbeat(heartbeatMessage.str());

            lastHearbeat = now;
        }while(0);
        // check peers if one is down
        for(std::map<std::string, peer>::iterator pit = peers.begin(); pit != peers.end();) {
            std::map<std::string, peer>::iterator backup = pit;
            backup++;

            if(pit->second.lastHearbeatInMs != 0 && (nowms - pit->second.lastHearbeatInMs) > (this->heartbeatIntervalInSeconds*3)){
                // notify peer is down
                std::set<Kraken::ISubscriber *>::iterator it = this->subscribers.begin();
                for(;it!=this->subscribers.end();it++){
                    (*it)->OnPeerDown(pit->second);
                }
                
                peers.erase(pit);
            }

            pit = backup;
        }

        // create udp server listen sockets
        do{
            if(this->socksrv_inaddr_anyv4!=-1){ break; }

            CloseOnDestruct<int> sock = socket(AF_INET,SOCK_DGRAM,0);

            if(sock==-1){ break; }

            const int yes = 1;
            struct sockaddr_in servaddr;

            bzero(&servaddr,sizeof(servaddr));

            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
            servaddr.sin_port=htons(this->port);

            setsockopt(sock, SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int));

            if(bind(sock ,(struct sockaddr *)&servaddr,sizeof(servaddr))==-1){
                break;
            }

            sock.disable();
            this->socksrv_inaddr_anyv4 = sock;
        }while(0);
        do{
            if(this->socksrv_inaddr_anyv6!=-1){ break; }

            CloseOnDestruct<int> sock = socket(AF_INET6,SOCK_DGRAM,0);

            if(sock==-1){ break; }

            const int yes = 1;
            struct sockaddr_in6 servaddr;

            bzero(&servaddr,sizeof(servaddr));

            servaddr.sin6_family = AF_INET6;
            servaddr.sin6_addr = in6addr_any;
            servaddr.sin6_port = htons(this->port);

            setsockopt(sock, SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int));

            if(bind(sock, (struct sockaddr *)&servaddr,sizeof(servaddr))==-1){
                break;
            }

            sock.disable();
            this->socksrv_inaddr_anyv6 = sock;
        }while(0);
        do{
            if(this->socksrv_mcastv4!=-1){ break; }

            struct addrinfo*  multicastAddr;     /* Multicast Address */
            struct addrinfo   hints  = { 0 };    /* Hints for name lookup */
            if (getaddrinfo(this->v4McastIp.c_str(), NULL, &hints, &multicastAddr) != 0){
                break;
            }

            CloseOnDestruct<int> sock = socket(AF_INET,SOCK_DGRAM,0);
            if(sock==-1){ break; }

            const int yes = 1;
            struct sockaddr_in servaddr;

            bzero(&servaddr,sizeof(servaddr));

            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
            servaddr.sin_port=htons(this->port);

            setsockopt(sock, SOL_SOCKET,SO_REUSEADDR, (char*)&yes,sizeof(int));

            if(bind(sock,(struct sockaddr *)&servaddr,sizeof(servaddr))==-1){
                freeaddrinfo(multicastAddr);
                break;
            }

            struct ip_mreq multicastRequest;  /* Multicast address join structure */

            /* Specify the multicast group */
            memcpy(&multicastRequest.imr_multiaddr,
                &((struct sockaddr_in*)(multicastAddr->ai_addr))->sin_addr,
                sizeof(multicastRequest.imr_multiaddr));

            /* Accept multicast from any interface */
            multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);

            /* Join the multicast address */
            if ( setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &multicastRequest, sizeof(multicastRequest)) != 0 ){
                freeaddrinfo(multicastAddr);
                break;
            }

            freeaddrinfo(multicastAddr);

            sock.disable();
            this->socksrv_mcastv4 = sock;
        }while(0);
        do{
            if(this->socksrv_mcastv6!=-1){ break; }

            struct addrinfo*  multicastAddr;     /* Multicast Address */
            struct addrinfo   hints  = { 0 };    /* Hints for name lookup */
            if (getaddrinfo(this->v6McastIp.c_str(), NULL, &hints, &multicastAddr) != 0){
                break;
            }

            CloseOnDestruct<int> sock = socket(AF_INET6,SOCK_DGRAM,0);
            if(sock==-1){ break; }

            const int yes = 1;
            struct sockaddr_in6 servaddr;

            bzero(&servaddr,sizeof(servaddr));

            servaddr.sin6_family = AF_INET6;
            servaddr.sin6_flowinfo = 0;
            servaddr.sin6_port = htons(this->port);
            servaddr.sin6_addr = in6addr_any;

            setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int));

            if(bind(sock,(struct sockaddr *)&servaddr,sizeof(servaddr))==-1){
                freeaddrinfo(multicastAddr);
                break;
            }

            struct ipv6_mreq multicastRequest;  /* Multicast address join structure */

            /* Specify the multicast group */
            memcpy(&multicastRequest.ipv6mr_multiaddr,
                 &((struct sockaddr_in6*)(multicastAddr->ai_addr))->sin6_addr,
                 sizeof(multicastRequest.ipv6mr_multiaddr));

            /* Accept multicast from any interface */
            multicastRequest.ipv6mr_interface = 0;

            /* Join the multicast address */
            if ( setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char*) &multicastRequest, sizeof(multicastRequest)) != 0 ){
                freeaddrinfo(multicastAddr);
                break;
            }
            freeaddrinfo(multicastAddr);

            sock.disable();
            this->socksrv_mcastv6 = sock;
        }while(0);

        {
            int nfds = -1;
            fd_set rfds;
            FD_ZERO(&rfds);

            if(this->socksrv_inaddr_anyv4!=-1){
                FD_SET(this->socksrv_inaddr_anyv4, &rfds);
                if(nfds<this->socksrv_inaddr_anyv4){ nfds = this->socksrv_inaddr_anyv4; }
            }
            if(this->socksrv_inaddr_anyv6!=-1){
                FD_SET(this->socksrv_inaddr_anyv6, &rfds);
                if(nfds<this->socksrv_inaddr_anyv6){ nfds = this->socksrv_inaddr_anyv6; }
            }
            if(this->socksrv_mcastv4!=-1){
                FD_SET(this->socksrv_mcastv4, &rfds);
                if(nfds<this->socksrv_mcastv4){ nfds = this->socksrv_mcastv4; }
            }
            if(this->socksrv_mcastv6!=-1){
                FD_SET(this->socksrv_mcastv6, &rfds);
                if(nfds<this->socksrv_mcastv6){ nfds = this->socksrv_mcastv6; }
            }
            nfds++;

            struct timeval tv;
            tv.tv_sec  = 1;
            tv.tv_usec = 0;
            int retval = select(1, &rfds, NULL, NULL, &tv);
            if(retval>0){
                char buffer[1500]; // default mtu size
                struct sockaddr_storage remote;
                socklen_t rsize = sizeof(remote);

                if(FD_ISSET(this->socksrv_inaddr_anyv4, &rfds)){
                    int len = recvfrom(this->socksrv_inaddr_anyv4, buffer, sizeof(buffer), 0, (struct sockaddr *)&remote, &rsize);
                    if(len>0){
                        std::string message(buffer, len);
                        this->parseHeartbeat(message, remote);
                    }
                }
                if(FD_ISSET(this->socksrv_inaddr_anyv6, &rfds)){
                    int len = recvfrom(this->socksrv_inaddr_anyv6, buffer, sizeof(buffer), 0, (struct sockaddr *)&remote, &rsize);
                    if(len>0){
                        std::string message(buffer, len);
                        this->parseHeartbeat(message, remote);
                    }
                }
                if(FD_ISSET(this->socksrv_mcastv4, &rfds)){
                    int len = recvfrom(this->socksrv_mcastv4, buffer, sizeof(buffer), 0, (struct sockaddr *)&remote, &rsize);
                    if(len>0){
                        std::string message(buffer, len);
                        this->parseHeartbeat(message, remote);
                    }
                }
                if(FD_ISSET(this->socksrv_mcastv6, &rfds)){
                    int len = recvfrom(this->socksrv_mcastv6, buffer, sizeof(buffer), 0, (struct sockaddr *)&remote, &rsize);
                    if(len>0){
                        std::string message(buffer, len);
                        this->parseHeartbeat(message, remote);
                    }
                }
            }
        }
    }
    this->isrunning = false;
}

void Kraken::parseHeartbeat(std::string message, struct sockaddr_storage &remote){
    std::string::size_type firstsep = message.find(":");
    std::string::size_type lastsep = message.find(":");
    if(!(firstsep < lastsep)){
        return;
    }

    std::string::size_type sz;
    int rport = std::stoi (message, &sz);
    std::string rns = message.substr(firstsep+1, lastsep-firstsep-1);
    std::string ridentifier = message.substr(lastsep+1);

    switch(remote.ss_family){
        case AF_INET6:
            ((struct sockaddr_in6*)(&remote))->sin6_port = htons(rport);
            break;
        default:
            ((struct sockaddr_in*)(&remote))->sin_port = htons(rport);
            break;
    }

    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    std::map<std::string, peer>::iterator pit;
    std::string key(ns+":"+identifier);
    pit = peers.find(key);
    if(pit==peers.end()){
        peer p;
        p.addr = remote;

        p.lastHearbeatInMs = ms;

        p.ns = ns;
        p.identifier = identifier;

        std::pair<std::map<std::string, peer>::iterator, bool> inserted = peers.insert(std::make_pair(key, p));
        pit = inserted.first;

        // notify about the new peer
        std::set<Kraken::ISubscriber *>::iterator it = this->subscribers.begin();
        for(;it!=this->subscribers.end();it++){
            (*it)->OnPeerUp(pit->second);
        }

    }

    pit->second.addr = remote;
    pit->second.lastHearbeatInMs = ms;
}

void Kraken::broadcastHeartbeat(std::string message){
    int i=0, ifc_num=0, broadcastPermission=1;
    struct ifconf ifc;
    struct ifreq ifr[10];

    struct sockaddr_in srcaddr;
    struct sockaddr_in broadcastAddr; /* Broadcast address */

    srcaddr.sin_family = AF_INET;
    srcaddr.sin_addr.s_addr= htonl(INADDR_ANY);
    srcaddr.sin_port=htons(this->port); //source port for outgoing packets

    CloseOnDestruct<int> sock = socket(AF_INET,SOCK_DGRAM,0);
    if(sock<0){ return; }

    bind(sock, (struct sockaddr *) &srcaddr, sizeof(srcaddr));
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission));

    memset(&broadcastAddr, 0, sizeof(broadcastAddr));   /* Zero out structure */
    broadcastAddr.sin_family = AF_INET;                 /* Internet address family */
    broadcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");   /* Broadcast IP address */
    broadcastAddr.sin_port = htons(this->port);      /* Broadcast port */

    // broadcast to 255.255.255.255
    {
        sendto(sock, message.data(), message.size(), 0, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr));
    }

    // from http://stackoverflow.com/questions/4139405/how-to-know-ip-address-for-interfaces-in-c
    {
        ifc.ifc_len = sizeof(ifr);
        ifc.ifc_ifcu.ifcu_buf = (caddr_t)ifr;

        if (ioctl(sock, SIOCGIFCONF, &ifc) == 0)
        {
            ifc_num = ifc.ifc_len / sizeof(struct ifreq);

            for (i = 0; i < ifc_num; ++i)
            {
                if (ifr[i].ifr_addr.sa_family != AF_INET)
                {
                    continue;
                }

                if (ioctl(sock, SIOCGIFBRDADDR, &ifr[i]) == 0)
                {
                    broadcastAddr.sin_addr.s_addr = ((struct sockaddr_in *)(&ifr[i].ifr_broadaddr))->sin_addr.s_addr;
                    sendto(sock, message.data(), message.size(), 0, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr));
                }
            }                      
        }
    }
}
void Kraken::multicastHeartbeat(std::string message){
    for(int i=0;i<2;i++){
      struct addrinfo* multicastAddr = NULL;
      //int       multicastTTL=1;           /* Arg: TTL of multicast packets */

      struct addrinfo hints = { 0 };    /* Hints for name lookup */

      /* Resolve destination address for multicast datagrams */
      hints.ai_family   = PF_UNSPEC;
      hints.ai_socktype = SOCK_DGRAM;
      hints.ai_flags    = AI_NUMERICHOST;
      int status;
      if ((status = getaddrinfo(i==0 ? this->v4McastIp.c_str() : this->v6McastIp.c_str(), NULL, &hints, &multicastAddr)) != 0 )
      {
          perror("getaddrinfo() failed");
          continue;
      }

      switch(multicastAddr->ai_family){
          case AF_INET6:
              ((struct sockaddr_in6*)(multicastAddr->ai_addr))->sin6_port = htons(this->port);
              break;
          default:
              ((struct sockaddr_in*)(multicastAddr->ai_addr))->sin_port = htons(this->port);
              break;
      }

      /* Create socket for sending multicast datagrams */
      CloseOnDestruct<int> sock = socket(multicastAddr->ai_family, multicastAddr->ai_socktype,0);
      if ( sock < 0 ){
        perror("socket() failed");
        continue;
      }

      /* Set TTL of multicast packet */
      //if ( setsockopt(sock,
      //        multicastAddr->ai_family == AF_INET6 ? IPPROTO_IPV6        : IPPROTO_IP,
      //        multicastAddr->ai_family == AF_INET6 ? IPV6_MULTICAST_HOPS : IP_MULTICAST_TTL,
      //        (char*) &multicastTTL, sizeof(multicastTTL)) != 0 )
      //  perror("setsockopt() failed");

      /* set the sending interface */
      /* FIXME does it have to be a ipv6 iface in case we're doing ipv6? */
      in_addr_t iface = INADDR_ANY;

      if(setsockopt (sock, 
             multicastAddr->ai_family == AF_INET6 ? IPPROTO_IPV6 : IPPROTO_IP,
             multicastAddr->ai_family == AF_INET6 ? IPV6_MULTICAST_IF : IP_MULTICAST_IF,
             (char*)&iface, sizeof(iface)) != 0)  
        perror("interface setsockopt()");

       sendto(sock, message.data(), message.size(), 0,
		  multicastAddr->ai_addr, multicastAddr->ai_addrlen);

      freeaddrinfo(multicastAddr);
    }
}
