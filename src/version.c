#include "version.h"

#define __BDATE__      "2019-10-07 22:40:01"
#define __BVERSION__   "1.4.011"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
