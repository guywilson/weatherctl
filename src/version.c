#include "version.h"

#define __BDATE__      "2019-09-27 09:13:47"
#define __BVERSION__   "1.4.002"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
