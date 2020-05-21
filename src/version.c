#include "version.h"

#define __BDATE__      "2020-05-21 16:58:31"
#define __BVERSION__   "1.8.001"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
