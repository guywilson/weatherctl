#include "version.h"

#define __BDATE__      "2019-10-11 22:32:43"
#define __BVERSION__   "1.4.019"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
