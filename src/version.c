#include "version.h"

#define __BDATE__      "2019-11-01 15:12:00"
#define __BVERSION__   "1.6.008"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
