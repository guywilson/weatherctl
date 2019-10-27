#include "version.h"

#define __BDATE__      "2019-10-27 19:16:09"
#define __BVERSION__   "1.6.002"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
