#include "version.h"

#define __BDATE__      "2021-11-11 15:40:20"
#define __BVERSION__   "1.8.004"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
