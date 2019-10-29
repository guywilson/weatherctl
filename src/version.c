#include "version.h"

#define __BDATE__      "2019-10-29 20:24:26"
#define __BVERSION__   "1.6.006"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
