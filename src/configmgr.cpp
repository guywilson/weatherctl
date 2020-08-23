#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <syslog.h>
#include <errno.h>

#include <map>
#include <vector>

#include "configmgr.h"
#include "wctl_error.h"

extern "C" {
#include "strutils.h"
}

using namespace std;

void ConfigManager::initialise(char * pszConfigFileName)
{
    strcpy(this->szConfigFileName, pszConfigFileName);

    readConfig();
}

void ConfigManager::readConfig()
{
    values.clear();

	FILE *			fptr;
	char *			pszConfigLine;
    char *          pszKey;
    char *          pszValue;
    char *          pszUntrimmedValue;
	char *			config = NULL;
    char *          reference = NULL;
	char *			pszCfgItem;
    char *          pszCfgItemFile;
	int				fileLength = 0;
	int				bytesRead = 0;
    int             i;
    int             j;
    int             valueLen = 0;
    int             delimPos = 0;
	const char *	delimiters = "\n\r";

	fptr = fopen(szConfigFileName, "rt");

	if (fptr == NULL) {
		syslog(LOG_ERR, "Failed to open config file %s with error %s", szConfigFileName, strerror(errno));
		throw wctl_error(wctl_error::buildMsg("ERROR reading config file %s with error %s", szConfigFileName, strerror(errno)), __FILE__, __LINE__);
	}

    fseek(fptr, 0L, SEEK_END);
    fileLength = ftell(fptr);
    rewind(fptr);

	config = (char *)malloc(fileLength + 1);

	if (config == NULL) {
		syslog(LOG_ERR, "Failed to alocate %d bytes for config file %s", fileLength, szConfigFileName);
		throw wctl_error(wctl_error::buildMsg("Failed to allocate %d bytes for config file %s", fileLength, szConfigFileName), __FILE__, __LINE__);
	}

    /*
    ** Retain pointer to original config buffer for 
    ** calling the re-entrant strtok_r() library function...
    */
    reference = config;

	/*
	** Read in the config file...
	*/
	bytesRead = fread(config, 1, fileLength, fptr);

	if (bytesRead != fileLength) {
		syslog(LOG_ERR, "Read %d bytes, but config file %s is %d bytes long", bytesRead, szConfigFileName, fileLength);
		throw wctl_error(
            wctl_error::buildMsg(
                "Failed to read config file, read %d bytes, but config file %s is %d bytes long", 
                bytesRead, 
                szConfigFileName, 
                fileLength), 
            __FILE__, 
            __LINE__);
	}

	fclose(fptr);

    /*
    ** Null terminate the string...
    */
    config[fileLength] = 0;

	pszConfigLine = strtok_r(config, delimiters, &reference);

	while (pszConfigLine != NULL) {
        if (pszConfigLine[0] == '#') {
            /*
            ** Ignore line comments...
            */
            pszConfigLine = strtok_r(NULL, delimiters, &reference);
            continue;
        }

        if (strlen(pszConfigLine) > 0) {
            for (i = 0;i < (int)strlen(pszConfigLine);i++) {
                if (pszConfigLine[i] == '=') {
                    pszKey = strndup(pszConfigLine, i);
                    delimPos = i;
                }
                if (delimPos) {
                    valueLen = strlen(pszConfigLine) - delimPos;

                    for (j = delimPos + 1;j < (int)strlen(pszConfigLine);j++) {
                        if (pszConfigLine[j] == '#') {
                            valueLen = (j - delimPos - 1);
                            break;
                        }
                    }

                    pszUntrimmedValue = strndup(&pszConfigLine[delimPos + 1], valueLen);
                    pszValue = str_trim_trailing(pszUntrimmedValue);

                    /*
                    ** Read the value from the file specified between <>...
                    */
                    if (pszValue[0] == '<' && str_endswith(pszValue, ">")) {
                        pszValue[strlen(pszValue) - 1] = 0;
                        pszCfgItemFile = str_trim(&pszValue[1]);

                        free(pszValue);  

                        FILE * f = fopen(pszCfgItemFile, "rt");

                        if (f == NULL) {
                            syslog(LOG_ERR, "Failed to open cfg item file %s with error %s", pszCfgItemFile, strerror(errno));
                            throw wctl_error(wctl_error::buildMsg("ERROR reading config item file %s: %s", pszCfgItemFile, strerror(errno)), __FILE__, __LINE__);
                        }

                        fseek(f, 0L, SEEK_END);
                        long propFileLength = ftell(f);
                        rewind(f);

                        pszCfgItem = (char *)malloc(propFileLength + 1);

                        if (pszCfgItem == NULL) {
                            syslog(LOG_ERR, "Failed to alocate %ld bytes for config item %s", propFileLength + 1, pszKey);
                            throw wctl_error(wctl_error::buildMsg("Failed to allocate %ld bytes for config item %s", propFileLength + 1, pszKey), __FILE__, __LINE__);
                        }

                        fread(pszCfgItem, propFileLength, 1, f);
                        pszCfgItem[propFileLength] = 0;
						
						/*
						** Trim the value read from the property file
						*/
						pszValue = str_trim(pszCfgItem);
						
						free(pszCfgItem);

                        fclose(f);
                    }
                    free(pszUntrimmedValue);
                    break;
                }
            }

            delimPos = 0;
            valueLen = 0;

            values[string(pszKey)] = string(pszValue);

            free(pszKey);
            free(pszValue);
        }

        pszConfigLine = strtok_r(NULL, delimiters, &reference);
	}

	free(config);

    isConfigured = true;
}

const char * ConfigManager::getValue(const char * key)
{
    if (!isConfigured) {
        readConfig();
    }

    if (values.count(key) > 0) {
        string & value = values[key];
        return value.c_str();
    }
    else {
		return "";
    }
}

bool ConfigManager::getValueAsBoolean(const char * key)
{
    const char *        pszValue;

    pszValue = getValue(key);

    return ((strcmp(pszValue, "yes") == 0 || strcmp(pszValue, "true") == 0 || strcmp(pszValue, "on") == 0) ? true : false);
}

int ConfigManager::getValueAsInteger(const char * key)
{
    const char *        pszValue;

    pszValue = getValue(key);

    return atoi(pszValue);
}

void ConfigManager::dumpConfig()
{
    readConfig();

    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        printf("'%s' = '%s'\n", it->first.c_str(), it->second.c_str());
    }
}