#include <exception>
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

using namespace std;

#ifndef _WCTL_ERROR
#define _WCTL_ERROR

class wctl_error : public exception
{
    private:
        string      message;

    public:
        wctl_error() {
            this->message.assign("WCTL error");
        }

        wctl_error(const char * msg) {
            this->message.assign("WCTL error: ");
            this->message.append(msg);
        }

        wctl_error(const char * msg, const char * file, int line) {
            char lineNumBuf[8];

            sprintf(lineNumBuf, ":%d", line);

            this->message.assign("WCTL error: ");
            this->message.append(msg);
            this->message.append(" at ");
            this->message.append(file);
            this->message.append(lineNumBuf);
        }

        virtual const char * what() const noexcept {
            return this->message.c_str();
        }

        static const char * buildMsg(const char * fmt, ...) {
            va_list     args;
            char *      buffer;

            va_start(args, fmt);
            
            buffer = (char *)malloc(strlen(fmt) + 80);
            
            vsprintf(buffer, fmt, args);
            
            va_end(args);

            return buffer;
        }
};

#endif
