#include "version.h"

#define __BDATE__      "2019-11-27 21:47:36"
#define __BVERSION__   "1.6.012"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
