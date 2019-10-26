#include "version.h"

#define __BDATE__      "2019-10-24 18:30:57"
#define __BVERSION__   "1.5.005"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
