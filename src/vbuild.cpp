#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "currenttime.h"

static const char * pszVersionCode = 
    "#define __BDATE__      \"<BUILD_DATE>\"\n" \
    "#define __BVERSION__   \"<BUILD_VERSION>\"\n" \
    "\n" \
    "const char * getWCTLVersion()\n" \
    "{\n" \
    "    return __BVERSION__;\n" \
    "}\n" \
    "\n" \
    "const char * getWCTLBuildDate()\n" \
    "{\n" \
    "    return __BDATE__;\n" \
    "}\n";

int main(int argc, char * argv[])
{
    FILE *		fptr;
    char		szIncrementalVersion[8];
    char        szVersionStr[64];
    char *      pszTimestamp;
    char *      pszIncrementalVersionFileName;
    char *      pszVersionCodeFileName;
    int			i = 0;
    int         majorVersion = 0;
    int         minorVersion = 0;
    int         incrementalVersion;

    if (argc > 3) {
		for (i = 1;i < argc;i++) {
			if (argv[i][0] == '-') {
				if (strncmp(&argv[i][1], "inc", 3) == 0) {
                    pszIncrementalVersionFileName = strdup(&argv[++i][0]);
				}
				else if (strncmp(&argv[i][1], "out", 3) == 0) {
                    pszVersionCodeFileName = strdup(&argv[++i][0]);
				}
                else if (strncmp(&argv[i][1], "major", 5) == 0) {
                    majorVersion = atoi(&argv[++i][0]);
                }
                else if (strncmp(&argv[i][1], "minor", 5) == 0) {
                    minorVersion = atoi(&argv[++i][0]);
                }
			}
		}
	}
	else {
		printf("Usage:\n\n");
		printf("vbuild <options>\n");
        printf("\t-inc [incremental version file]\n");
        printf("\t-out [version source code output]\n");
        printf("\t-major [major version]\n");
        printf("\t-minor [minor version]\n\n");
		return -1;
	}

    fptr = fopen(pszIncrementalVersionFileName, "rt");

    if (fptr == NULL) {
        printf("Failed to open incremental version file %s for reading\n\n", pszIncrementalVersionFileName);
        return -1;
    }

    i = 0;

    while (!feof(fptr)) {
        szIncrementalVersion[i++] = fgetc(fptr);
    }
    szIncrementalVersion[i] = 0;

    fclose(fptr);

    fptr = fopen(pszIncrementalVersionFileName, "wt");

    if (fptr == NULL) {
        printf("Failed to open incremental version file %s for writing\n\n", pszIncrementalVersionFileName);
        return -1;
    }

    incrementalVersion = atoi(szIncrementalVersion);
    incrementalVersion++;

    fprintf(fptr, "%03d", incrementalVersion);
    fclose(fptr);

    sprintf(
        szVersionStr, 
        "%d.%d.%03d", 
        majorVersion, 
        minorVersion, 
        incrementalVersion);

    fptr = fopen(pszVersionCodeFileName, "wt");

    if (fptr == NULL) {
        printf("Failed to open code file %s\n\n", pszVersionCodeFileName);
        return -1;
    }

    CurrentTime time;
    pszTimestamp = time.getTimeStamp();

    i = 0;

    while (pszVersionCode[i] != 0) {
        if (strncmp(&pszVersionCode[i], "<BUILD_DATE>", 12) == 0) {
            fwrite(pszTimestamp, 1, strlen(pszTimestamp), fptr);
            i += 12;
        }
        else if (strncmp(&pszVersionCode[i], "<BUILD_VERSION>", 15) == 0) {
            fwrite(szVersionStr, 1, strlen(szVersionStr), fptr);
            i += 15;
        }
        else {
            fputc((int)pszVersionCode[i], fptr);
            i++;
        }
    }

    fclose(fptr);

    return 0;
}