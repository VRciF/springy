#include "settings.hpp"

#include "util/synchronized.hpp"

namespace Springy{
    Settings::Settings(std::string id){
        this->bOverwriteSettings = true;
        this->localStorage["id"] = id;
    }
    Settings::~Settings(){}

    boost::logic::tribool Settings::overwriteSettings(){
        Synchronized ro(this->bOverwriteSettings);

        return this->bOverwriteSettings;
    }
    Settings& Settings::overwriteSettings(boost::logic::tribool bOverwriteSettings){
        Synchronized ro(this->bOverwriteSettings);

        if(bOverwriteSettings == boost::indeterminate){
            throw std::runtime_error("invalid argument given");
        }

        this->bOverwriteSettings = bOverwriteSettings;

        return *this;
    }

    template<typename T> T Settings::option(std::string name){
        Synchronized local(this->localStorage);

        storage::iterator it = this->localStorage.find(name);
        if(it==this->localStorage.end()){
            Synchronized syncToken(this->globalStorage);
            it = this->globalStorage.find(name);
        }
        if(it==this->localStorage.end()){
            throw std::runtime_error(std::string("unkown option given: ")+name);
        }

        boost::any rval = it->second;

        return boost::any_cast<T>(rval);
    }

    Settings& Settings::option(std::string name, boost::any value, boost::logic::tribool bOverwriteSettings){
        Synchronized local(this->localStorage);

        storage::iterator it = this->localStorage.find(name);

        if(it==this->localStorage.end() || it->second.empty() || bOverwriteSettings || this->bOverwriteSettings){
            this->localStorage[name] = value;
        }

        return *this;
    }

    template<typename T> T& Settings::operator[](std::string name){
        Synchronized local(this->localStorage);
        storage::iterator it = this->localStorage.find(name);
        if(it==this->localStorage.end()){
            Synchronized syncToken(this->globalStorage);
            it = this->globalStorage.find(name);
            std::pair<storage::iterator, bool> irval = this->localStorage.insert(std::make_pair(name, boost::any()));
            if(irval.second == false){
                throw std::runtime_error("creating settings entry failed");
            }
            it = irval.first;
        }

        return boost::any_cast<T&>(it->second);
    }

    // known options
    std::string Settings::id(){
        Synchronized local(this->localStorage);
        return boost::any_cast<std::string>(this->localStorage["id"]);
    }
    Settings& Settings::id(std::string value){
        Synchronized local(this->localStorage);
        this->localStorage["id"] = value;
        return *this;
    }
}
