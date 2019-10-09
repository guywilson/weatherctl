#include "version.h"

#define __BDATE__      "2019-10-09 15:31:23"
#define __BVERSION__   "1.4.013"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
