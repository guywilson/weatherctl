#include "version.h"

#define __BDATE__      "2019-10-06 17:24:39"
#define __BVERSION__   "1.4.009"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
