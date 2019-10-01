#include "version.h"

#define __BDATE__      "2019-10-01 17:12:44"
#define __BVERSION__   "1.4.004"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
