#include "version.h"

#define __BDATE__      "2019-10-10 10:28:43"
#define __BVERSION__   "1.4.014"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
