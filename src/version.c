#include "version.h"

#define __BDATE__      "2019-10-17 15:11:03"
#define __BVERSION__   "1.5.002"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
