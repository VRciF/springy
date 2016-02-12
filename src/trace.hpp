#ifndef SPRINGY_TRACE
#define SPRINGY_TRACE

#include "util/synchronized.hpp"

#include <map>
#include <vector>
#include <chrono>
#include <sstream>

namespace Springy{
    class Trace{
        protected:
            Trace();

            struct entry{
                const char *file;
                const char *method;
                int line;
                std::chrono::system_clock::time_point now;
                int err;

                std::string msg;
            };

            int depthIdx;
            int *referenceCounter;

            void log(struct entry e);

        public:
            static std::map<std::thread::id, std::vector<struct entry> > currentTraces;
            static std::map<std::thread::id, int> traceDepth;

            Trace(const char *file, const char *method, int line);
            Trace(const Trace& t);
            ~Trace();

            void log(const char *file, const char *method, int line, std::string msg=std::string());
            void log();

            template<typename T>
            Trace& operator<<(T &arg){
                std::thread::id current_thread_id = std::this_thread::get_id();

                Synchronized syncToken(Trace::currentTraces);

                std::ostringstream o;
                o << arg;
                if(Trace::traceDepth.find(current_thread_id) != Trace::traceDepth.end() &&
                   Trace::currentTraces[current_thread_id].size() > (std::size_t)Trace::traceDepth[current_thread_id]){
                    Trace::currentTraces[current_thread_id][Trace::traceDepth[current_thread_id]].msg.append(o.str());
                }

                return *this;
            }
    };
}
#endif
