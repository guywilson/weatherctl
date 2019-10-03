#include "version.h"

#define __BDATE__      "2019-10-03 08:50:23"
#define __BVERSION__   "1.4.006"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
