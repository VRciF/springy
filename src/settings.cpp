#include "settings.hpp"

#include "libc/ilibc.hpp"

namespace Springy{
    Settings::Settings(Springy::LibC::ILibC *libc) : volumes(libc) {
        this->httpdPort = 0;
    }
/*
    Settings::Settings(std::string id){
        this->bOverwriteSettings = true;
        this->localStorage["id"] = id;
        this->localStorage["directories"] = std::ref(this->directories);
    }
    Settings::~Settings(){}

    boost::logic::tribool Settings::overwriteSettings(){
        Synchronized ro(this->bOverwriteSettings, Synchronized::LockType::READ);

        return this->bOverwriteSettings;
    }
    Settings& Settings::overwriteSettings(boost::logic::tribool bOverwriteSettings){
        Synchronized ro(this->bOverwriteSettings);

        if(bOverwriteSettings == boost::indeterminate){
            throw Springy::Exception("invalid argument given", __FILE__, __LINE__);
        }

        this->bOverwriteSettings = bOverwriteSettings;

        return *this;
    }

    bool Settings::exists(std::string name){
        {
            Synchronized local(this->localStorage, Synchronized::LockType::READ);
            if(this->localStorage.find(name)!=this->localStorage.end()){ return true; }
        }
        {
            Synchronized global(this->globalStorage, Synchronized::LockType::READ);
            if(this->globalStorage.find(name)!=this->globalStorage.end()){ return true; }
        }

        return false;
    }

    Settings& Settings::option(std::string name, boost::any value, boost::logic::tribool bOverwriteSettings){
        Synchronized local(this->localStorage);

        storage::iterator it = this->localStorage.find(name);

        if(it==this->localStorage.end() || it->second.empty() || bOverwriteSettings || this->bOverwriteSettings){
            this->localStorage[name] = value;
        }

        return *this;
    }

    // known options
    std::string Settings::id(){
        Synchronized local(this->localStorage, Synchronized::LockType::READ);
        return boost::any_cast<std::string>(this->localStorage["id"]);
    }
    Settings& Settings::id(std::string value){
        Synchronized local(this->localStorage, Synchronized::LockType::READ);
        this->localStorage["id"] = value;
        return *this;
    }
*/
}

