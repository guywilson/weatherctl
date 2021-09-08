#include "version.h"

#define __BDATE__      "2021-09-08 16:31:31"
#define __BVERSION__   "1.8.003"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
