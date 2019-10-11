#include "version.h"

#define __BDATE__      "2019-10-11 17:50:56"
#define __BVERSION__   "1.4.017"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
