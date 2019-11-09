#include "version.h"

#define __BDATE__      "2019-11-09 21:32:35"
#define __BVERSION__   "1.6.011"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
