#include "version.h"

#define __BDATE__      "2019-10-07 21:21:48"
#define __BVERSION__   "1.4.010"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
