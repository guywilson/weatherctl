#include "version.h"

#define __BDATE__      "2019-10-10 17:30:39"
#define __BVERSION__   "1.4.015"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
