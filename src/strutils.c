#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "strutils.h"

char * str_trim_trailing(const char * str)
{
    int             i = 0;
    int             endPos = 0;

    if (str != NULL) {
        endPos = strlen(str) - 1;
        i = endPos;

        while (isspace(str[i--]) && i > 0);

        if (i > 0) {
            endPos = i + 1;
        }

        return strndup(str, endPos + 1);
    }
    else {
        return NULL;
    }
}

char * str_trim_leading(const char * str)
{
    int             i = 0;
    int             startPos = 0;
    int             endPos = 0;

    if (str != NULL) {
        endPos = strlen(str);

        while (isspace(str[i]) && i < endPos) {
            i++;
        }

        if (i > 0) {
            startPos = i;
        }
        else {
            endPos += 1;
        }

        return strndup(&str[startPos], endPos);
    }
    else {
        return NULL;
    }
}

char * str_trim(const char * str)
{
    char * strTrailing;
    char * strLeading;

    if (str != NULL) {
        strTrailing = str_trim_trailing(str);
        strLeading = str_trim_leading(strTrailing);

        free(strTrailing);

        return strLeading;
    }
    else {
        return NULL;
    }
}

int str_endswith(char * src, const char * suffix)
{
    int         isEndsWith = 0;
    int         searchPos = 0;

    if (strlen(suffix) > strlen(src)) {
        return -1;
    }

    searchPos = strlen(src) - strlen(suffix);

    if (strncmp(&src[searchPos], suffix, strlen(suffix)) == 0) {
        isEndsWith = 1;
    }

    return isEndsWith;
}
