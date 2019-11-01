#include "version.h"

#define __BDATE__      "2019-11-01 23:25:11"
#define __BVERSION__   "1.6.009"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
