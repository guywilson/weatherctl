#include "version.h"

#define __BDATE__      "2019-10-08 22:08:17"
#define __BVERSION__   "1.4.012"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
