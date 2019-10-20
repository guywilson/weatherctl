#include "version.h"

#define __BDATE__      "2019-10-20 16:38:13"
#define __BVERSION__   "1.5.003"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
