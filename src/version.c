#include "version.h"

#define __BDATE__      "2019-09-27 10:06:10"
#define __BVERSION__   "1.4.003"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
