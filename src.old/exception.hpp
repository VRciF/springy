#ifndef __SPRINGY_EXCEPTION_HPP__
#define __SPRINGY_EXCEPTION_HPP__

#include <sstream>
#include <stdexcept>

#include "trace.hpp"

namespace Springy{
    class Exception : public std::runtime_error {
        protected:
            std::string msg;
            const char *file;
            const char *method;
            int line;
            int err;

            Springy::Trace t;

            void init(const char *file, const char *method, int line, const std::string &arg){
                this->err = errno;
                this->file = file;
                this->method = method;
                this->line = line;

                if(arg.length()){
                    std::ostringstream o;
                    o << arg;
                    this->msg = o.str();
                    this->t << "Exception=";
                    this->t << arg;
                }
            }

        public:
            Exception(const char *file, int line) : Exception(file, "", line, "") {}
            Exception(const char *file, const char *method, int line) : Exception(file, method, line, "") {}
            Exception(const char *file, const char *method, int line, const std::string &arg) : std::runtime_error(arg), t(file, method, line) {
                this->init(file, method, line, arg);
            }

            ~Exception() throw() {}
            const char *what() const throw() {
                std::ostringstream o;
                o << file << ":" << method << ":" << line << "[" << err << ":" << strerror(err) << "]";
                if(this->msg.length()){
                    o << " : " << this->msg;
                }
                return o.str().c_str();
            }

            template<typename T>
            Exception& operator<<(T arg){
                if(msg.length()<=0){ this->t << "Exception="; }
                std::ostringstream o;
                o << arg;
                this->t << arg;
                msg.append(o.str());
                return *this;
            }
    };
}

#endif
