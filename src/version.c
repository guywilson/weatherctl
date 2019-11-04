#include "version.h"

#define __BDATE__      "2019-11-04 10:24:32"
#define __BVERSION__   "1.6.010"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
