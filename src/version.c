#include "version.h"

#define __BDATE__      "2019-10-05 21:47:23"
#define __BVERSION__   "1.4.008"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
