#include "version.h"

#define __BDATE__      "2021-11-11 16:06:24"
#define __BVERSION__   "1.8.005"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
