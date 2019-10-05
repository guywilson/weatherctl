#include "version.h"

#define __BDATE__      "2019-10-05 19:02:24"
#define __BVERSION__   "1.4.007"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
