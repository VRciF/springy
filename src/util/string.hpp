#ifndef SPRINGY_UTIL_STRING
#define SPRINGY_UTIL_STRING

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
        };
    }
}

#endif
