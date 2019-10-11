#include "version.h"

#define __BDATE__      "2019-10-11 13:22:03"
#define __BVERSION__   "1.4.016"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
