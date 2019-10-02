#include "version.h"

#define __BDATE__      "2019-10-02 19:47:30"
#define __BVERSION__   "1.4.005"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
