#include "version.h"

#define __BDATE__      "2019-09-24 22:33:42"
#define __BVERSION__   "1.3.009"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
