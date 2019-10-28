#include "version.h"

#define __BDATE__      "2019-10-28 11:53:00"
#define __BVERSION__   "1.6.005"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
