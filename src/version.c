#include "version.h"

#define __BDATE__      "2019-10-12 13:59:54"
#define __BVERSION__   "1.4.020"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
