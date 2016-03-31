#ifndef SPRINGY_UTIL_STRING
#define SPRINGY_UTIL_STRING

#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/algorithm/string.hpp>

namespace Springy{
    namespace Util{
        class String{
            public:
                static std::string urldecode(const std::string &src){
                    std::string dst;
                    char a, b, ch;
                    for(unsigned int i=0;i<src.length();i++) {
                            if ((src[i] == '%') && (src.length()-i)>=2 &&
                                ((a = src[i+1]) && (b = src[i+2])) &&
                                (isxdigit(a) && isxdigit(b))) {
                                    if (a >= 'a')
                                            a -= 'a'-'A';
                                    if (a >= 'A')
                                            a -= ('A' - 10);
                                    else
                                            a -= '0';
                                    if (b >= 'a')
                                            b -= 'a'-'A';
                                    if (b >= 'A')
                                            b -= ('A' - 10);
                                    else
                                            b -= '0';
                                    ch = 16*a+b;
                                    i+=2;
                            } else {
                                ch = src[i];
                            }
                            dst.append(1, ch);
                    }
                    return dst;
                }

                static std::string decode64(const std::string &val) {
                    using namespace boost::archive::iterators;
                    using It = transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
                    return boost::algorithm::trim_right_copy_if(std::string(It(std::begin(val)), It(std::end(val))), [](char c) {
                        return c == '\0';
                    });
                }

                static std::string encode64(const std::string &val) {
                    using namespace boost::archive::iterators;
                    using It = base64_from_binary<transform_width<std::string::const_iterator, 6, 8>>;
                    auto tmp = std::string(It(std::begin(val)), It(std::end(val)));
                    return tmp.append((3 - val.size() % 3) % 3, '=');
                }

                // trim from start
                static inline std::string &ltrim(std::string &s) {
                        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
                        return s;
                }
                // trim from end
                static inline std::string &rtrim(std::string &s) {
                        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
                        return s;
                }
                // trim from both ends
                static inline std::string &trim(std::string &s) {
                        return ltrim(rtrim(s));
                }
        };
    }
}

#endif
