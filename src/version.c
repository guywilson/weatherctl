#include "version.h"

#define __BDATE__      "2019-09-27 08:32:26"
#define __BVERSION__   "1.4.001"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
