#include "version.h"

#define __BDATE__      "2019-10-27 20:57:59"
#define __BVERSION__   "1.6.004"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
