#include "version.h"

#define __BDATE__      "2019-10-31 22:12:00"
#define __BVERSION__   "1.6.007"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
