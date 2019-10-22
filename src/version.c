#include "version.h"

#define __BDATE__      "2019-10-22 17:03:48"
#define __BVERSION__   "1.5.004"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
