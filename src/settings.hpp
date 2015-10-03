#ifndef SPRINGY_SETTINGS
#define SPRINGY_SETTINGS

#include <unordered_map>
#include <boost/any.hpp>
#include <boost/logic/tribool.hpp>

#include "util/synchronized.hpp"

namespace Springy{
    class Settings{
        protected:
            boost::logic::tribool bOverwriteSettings;

            typedef std::unordered_map<std::string, boost::any> storage;
            storage localStorage;
            storage globalStorage;

        public:
            Settings(std::string id="");
            ~Settings();

            boost::logic::tribool overwriteSettings();
            Settings& overwriteSettings(boost::logic::tribool bOverwriteSettings);

            bool exists(std::string name);

            template<typename T> T& option(std::string name){
                Synchronized local(this->localStorage, Synchronized::LockType::READ);

                storage::iterator it = this->localStorage.find(name);
                if(it==this->localStorage.end()){
                    Synchronized syncToken(this->globalStorage, Synchronized::LockType::READ);
                    it = this->globalStorage.find(name);
                }
                if(it==this->localStorage.end()){
                    throw std::runtime_error(std::string("unkown option given: ")+name);
                }

                boost::any &rval = it->second;

                return boost::any_cast<T&>(rval);
            }
            template<typename T> T& operator[](std::string name){
                return this->option<T&>(name);
            }

            Settings& option(std::string name, boost::any value, boost::logic::tribool bOverwriteSettings=boost::logic::tribool());

            // known options
            std::string id();
            Settings& id(std::string value);
    };
}
#endif
