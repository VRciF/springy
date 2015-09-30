#ifndef SPRINGY_SETTINGS
#define SPRINGY_SETTINGS

#include <unordered_map>
#include <boost/any.hpp>
#include <boost/logic/tribool.hpp>

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

            template<typename T> T option(std::string name);
            Settings& option(std::string name, boost::any value, boost::logic::tribool bOverwriteSettings=boost::logic::tribool());

            template<typename T> T& operator[](std::string name);

            // known options
            std::string id();
            Settings& id(std::string value);
    };
}
#endif
