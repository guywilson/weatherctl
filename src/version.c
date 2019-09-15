#include "version.h"

#define __BDATE__      "2019-09-15 17:20:06"
#define __BVERSION__   "1.2.034"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
