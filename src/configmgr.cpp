#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <syslog.h>

#include <map>
#include <vector>

#include "configmgr.h"
#include "exception.h"

extern "C" {
#include "strutils.h"
}

using namespace std;

void ConfigManager::initialise(char * pszConfigFileName)
{
    strcpy(this->szConfigFileName, pszConfigFileName);
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
	int				fileLength = 0;
	int				bytesRead = 0;
    int             i;
    int             j;
    int             valueLen = 0;
    int             delimPos = 0;
	const char *	delimiters = "\n\r";

	fptr = fopen(szConfigFileName, "rt");

	if (fptr == NULL) {
		syslog(LOG_ERR, "Failed to open config file %s", szConfigFileName);
		throw new Exception("ERROR reading config");
	}

    fseek(fptr, 0L, SEEK_END);
    fileLength = ftell(fptr);
    rewind(fptr);

	config = (char *)malloc(fileLength + 1);

	if (config == NULL) {
		syslog(LOG_ERR, "Failed to alocate %d bytes for config file %s", fileLength, szConfigFileName);
		throw new Exception("Failed to allocate memory for config.");
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
		throw new Exception("Failed to read in config file.");
	}

	fclose(fptr);

    /*
    ** Null terminate the string...
    */
    config[fileLength] = 0;

	syslog(LOG_INFO, "Read %d bytes from config file %s", bytesRead, szConfigFileName);

	pszConfigLine = strtok_r(config, delimiters, &reference);

	while (pszConfigLine != NULL) {
        if (pszConfigLine[0] == '#') {
            /*
            ** Ignore line comments...
            */
            pszConfigLine = strtok_r(NULL, delimiters, &reference);
            continue;
        }

        syslog(LOG_DEBUG, "Read %d chars of line '%s'", (int)strlen(pszConfigLine), pszConfigLine);

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
                    free(pszUntrimmedValue);
                    break;
                }
            }

            delimPos = 0;
            valueLen = 0;

            syslog(LOG_DEBUG, "Got key '%s' and value '%s'", pszKey, pszValue);

            values[string(pszKey)] = string(pszValue);

            free(pszKey);
            free(pszValue);
        }

        pszConfigLine = strtok_r(NULL, delimiters, &reference);
	}

	free(config);

    isConfigured = true;
}

string & ConfigManager::getValue(string key)
{
    if (!isConfigured) {
        readConfig();
    }

    string & value = values[key];

    return value;
}

string & ConfigManager::getValue(const char * key)
{
    return getValue(string(key));
}

const char * ConfigManager::getValueAsCstr(const char * key)
{
    return getValue(key).c_str();
}

bool ConfigManager::getValueAsBoolean(const char * key)
{
    const char *        pszValue;

    pszValue = getValueAsCstr(key);

    return ((strcmp(pszValue, "yes") == 0 || strcmp(pszValue, "true") == 0 || strcmp(pszValue, "on") == 0) ? true : false);
}

int ConfigManager::getValueAsInteger(const char * key)
{
    const char *        pszValue;

    pszValue = getValueAsCstr(key);

    return atoi(pszValue);
}

void ConfigManager::dumpConfig()
{
    readConfig();

    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        printf("'%s' = '%s'\n", it->first.c_str(), it->second.c_str());
    }
}