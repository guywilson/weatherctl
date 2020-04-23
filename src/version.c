#include "version.h"

#define __BDATE__      "2020-04-23 15:12:06"
#define __BVERSION__   "1.7.001"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
