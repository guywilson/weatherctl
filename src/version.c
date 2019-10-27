#include "version.h"

#define __BDATE__      "2019-10-27 20:37:07"
#define __BVERSION__   "1.6.003"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
