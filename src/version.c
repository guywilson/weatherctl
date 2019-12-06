#include "version.h"

#define __BDATE__      "2019-12-06 21:46:14"
#define __BVERSION__   "1.6.013"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
