#include "version.h"

#define __BDATE__      "2020-05-25 17:04:35"
#define __BVERSION__   "1.8.002"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
