#include "version.h"

#define __BDATE__      "2019-10-27 15:55:20"
#define __BVERSION__   "1.6.001"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
