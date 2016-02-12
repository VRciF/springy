#ifndef SPRINGY_UTIL_URI
#define SPRINGY_UTIL_URI

namespace Springy{
    namespace Util{
        class Uri{
            public:
                Uri(std::string uri){
                    this->proto[0] = this->proto[1]
                  = this->uname[0] = this->uname[1]
                  = this->pwd[0] = this->pwd[1]
                  = this->hst[0] = this->hst[1]
                  = this->prt[0] = this->prt[1]
                  = this->pth[0] = this->pth[1]
                  = this->qury[0] = this->qury[1]
                  = this->frgmnt[0] = this->frgmnt[1]
                  = 0;
                    this->uri = uri;
                    this->parse();
                }

                std::string protocol(){
                    std::size_t len = this->proto[1]-this->proto[0];
                    if(len <= 0){ return std::string(); }
                    return this->uri.substr(this->proto[0], len+1);
                }
                std::string username(){
                    std::size_t len = this->uname[1]-this->uname[0];
                    if(len <= 0){ return std::string(); }
                    return this->uri.substr(this->uname[0], len+1);
                }
                std::string password(){
                    std::size_t len = this->pwd[1]-this->pwd[0];
                    if(len <= 0){ return std::string(); }
                    return this->uri.substr(this->pwd[0], len+1);
                }
                std::string host(){
                    std::size_t len = this->hst[1]-this->hst[0];
                    if(len <= 0){ return std::string(); }
                    return this->uri.substr(this->hst[0], len+1);
                }
                int port(){
                    std::size_t len = this->prt[1]-this->prt[0];
                    if(len <= 0){ return 0; }
                    return std::stoi(this->uri.substr(this->prt[0], len+1));
                }
                std::string path(){
                    std::size_t len = this->pth[1]-this->pth[0];
                    if(len <= 0){ return std::string(); }
                    return this->uri.substr(this->pth[0], len+1);
                }
                std::string query(){
                    std::size_t len = this->qury[1]-this->qury[0];
                    if(len <= 0){ return std::string(); }
                    return this->uri.substr(this->qury[0], len+1);
                }
                std::string fragment(){
                    std::size_t len = this->frgmnt[1]-this->frgmnt[0];
                    if(len <= 0){ return std::string(); }
                    return this->uri.substr(this->frgmnt[0], len+1);
                }

                std::string string() const { return this->uri; }
                operator std::string() const { return this->string(); }

            protected:
                // the first entry is the position (index origin 0) of the first character for the given value
                // the second entry is the position (index origin 0) of the last character for the given value
                std::size_t proto[2], uname[2], pwd[2], hst[2],
                            prt[2], pth[2], qury[2], frgmnt[2];
                std::string uri;

                // prot://uname:pwd@host:port/path?query#fragment
                void parse(){
                    std::size_t nextTokenStart = 0;

                    this->proto[0] = 0;
                    this->proto[1] = this->uri.find("://");
                    if(this->proto[1] == std::string::npos){
                        this->proto[1] = 0;
                    }
                    else{
                        nextTokenStart = this->proto[1]+3;
                        this->proto[1]--;
                    }

                    // parse username:password@host part
                    std::size_t pathStart = this->uri.find('/', nextTokenStart);
                    std::size_t hostStart = this->uri.find('@', nextTokenStart);
                    
                    if(pathStart == std::string::npos){
                        pathStart = this->uri.length();
                    }

                    if(hostStart == std::string::npos || hostStart >= pathStart){
                        // a hostname with maybe a port is given like ://host:port OR ://host:port/
                        this->hst[0] = nextTokenStart;

                        std::size_t portStart = this->uri.find(':', nextTokenStart);
                        if(portStart == std::string::npos){
                            this->hst[1] = pathStart-1;
                        }
                        else{
                            this->hst[1] = portStart-1;
                            this->prt[0] = portStart+1;
                            this->prt[1] = pathStart-1;
                        }
                        if(pathStart == this->uri.length()){
                            return;
                        }
                    }
                    else{
                        // hostStart has a position - so check left (username:password) and right (host:port) parts
                        std::size_t passwordStart = this->uri.find(':', nextTokenStart);
                        std::size_t portStart = 0;
                        if(passwordStart == std::string::npos || passwordStart > hostStart){
                            portStart = passwordStart;
                        }
                        else{
                            portStart = this->uri.find(':', hostStart);
                        }
                        
                        // left side
                        this->uname[0] = nextTokenStart;
                        if(passwordStart != std::string::npos && passwordStart < hostStart){
                            this->uname[1] = passwordStart-1;
                            this->pwd[0] = passwordStart+1;
                            this->pwd[1] = hostStart-1;
                        }
                        else{
                            this->uname[1] = hostStart-1;
                        }

                        // right side
                        this->hst[0] = hostStart+1;
                        if(portStart != std::string::npos && portStart < pathStart){
                            this->hst[1] = portStart-1;
                            this->prt[0] = portStart+1;
                            this->prt[1] = pathStart-1;
                        }
                        else{
                            this->hst[1] = pathStart-1;
                        }
                    }
                    nextTokenStart = pathStart;

                    std::size_t queryStart = this->uri.find('?', nextTokenStart);
                    std::size_t fragmentStart = this->uri.find('#', nextTokenStart);

                    this->pth[0] = pathStart;
                    if(queryStart == std::string::npos && fragmentStart == std::string::npos){
                        this->pth[1] = this->uri.length()-1;
                    }
                    else if(queryStart != std::string::npos){
                        this->pth[1] = queryStart-1;
                        fragmentStart = this->uri.find('#', queryStart);
                        this->qury[0] = queryStart+1;
                        if(fragmentStart == std::string::npos){
                            this->qury[1] = this->uri.length()-1;
                        }
                        else{
                            this->qury[1] = fragmentStart-1;
                            this->frgmnt[0] = fragmentStart+1;
                            this->frgmnt[1] = this->uri.length()-1;
                        }

                        return;
                    }
                    else{
                        this->pth[1] = fragmentStart-1;
                            this->frgmnt[0] = fragmentStart+1;
                            this->frgmnt[1] = this->uri.length()-1;
                    }
                }
                
        };
    }
}

#endif
