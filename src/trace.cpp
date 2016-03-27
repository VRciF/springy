#include "trace.hpp"

#include <boost/exception/diagnostic_information.hpp> 

namespace Springy{
    std::map<std::thread::id, std::vector<struct Springy::Trace::entry> > Trace::currentTraces;
    std::map<std::thread::id, int> Trace::traceDepth;

    Trace::Trace(const char *file, const char *method, int line){
        //std::cout << file << ":" << method << ":" << line << std::endl;

        this->referenceCounter = new int;
        *this->referenceCounter = 0;

        struct entry e;
        e.file   = file;
        e.method = method;
        e.line   = line;
        e.now    = std::chrono::system_clock::now();
        e.err = errno;

        std::thread::id current_thread_id = std::this_thread::get_id();

        Synchronized syncToken(this->currentTraces);

        if(this->traceDepth.find(current_thread_id) == this->traceDepth.end()){
            this->traceDepth[current_thread_id] = 0;
        }
        else{
            this->traceDepth[current_thread_id]++;
        }

        this->depthIdx = this->traceDepth[current_thread_id];

        if(this->currentTraces[current_thread_id].size() > (std::size_t)depthIdx){
            this->currentTraces[current_thread_id].erase(this->currentTraces[current_thread_id].begin() + this->traceDepth[current_thread_id],
                                                         this->currentTraces[current_thread_id].end());
        }
        this->currentTraces[current_thread_id].push_back(e);
    }
    Trace::Trace(const Trace& t){
        this->depthIdx = t.depthIdx;
        this->referenceCounter = t.referenceCounter;
        (*this->referenceCounter)++;
    }
    Trace::~Trace(){
        if(*this->referenceCounter > 0){
            (*this->referenceCounter)--;
            return;
        }
        delete this->referenceCounter;

        std::thread::id current_thread_id = std::this_thread::get_id();

        Synchronized syncToken(this->currentTraces);

        this->traceDepth[current_thread_id]--;
    }

    void Trace::log(const char *file, const char *method, int line, std::string msg){
        this->log();

        struct entry e;
        e.file = file;
        e.line = line;
        e.method = method;
        e.msg = msg;

        // if a currently catched exception exists and 
        std::exception_ptr p = std::current_exception();
        if(p && std::string(p.__cxa_exception_type()->name()).find("Springy") == std::string::npos){
            e.msg.append(":").append(boost::current_exception_diagnostic_information());
        }

        e.err = errno;
        e.now = std::chrono::system_clock::now();
        this->log(e);
    }
    void Trace::log(){
        std::thread::id current_thread_id = std::this_thread::get_id();

        Synchronized syncToken(Trace::currentTraces);

        for(std::size_t i=this->depthIdx;i < Trace::currentTraces[current_thread_id].size();i++){
            struct entry &e = Trace::currentTraces[current_thread_id][i];
            this->log(e);
        }
    }

    void Trace::log(struct entry e){
        std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(e.now.time_since_epoch());

        std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(us);
        std::time_t t = s.count();
        std::size_t fractional_seconds = us.count() % 1000000;
        
        std::tm * ptm = std::localtime(&t);
        char buffer[32];
        // Format: Do, 01.01.1970 00:00:00
        std::strftime(buffer, 32, "%a, %Y-%m-%d %H:%M:%S", ptm);

        std::cout << e.file << ":" << e.method << ":" << e.line << "  [" << buffer << "." << fractional_seconds << "]";
        std::cout << " (" << e.err << "|" << strerror(e.err) << ")";
        if(e.msg.length()){
            std::cout << " " << e.msg;
        }
        std::cout << std::endl;
    }
}
