#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <map>
#include <vector>

#include "configmgr.h"
#include "exception.h"

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
	char *			config = NULL;
    char *          reference = NULL;
	int				fileLength = 0;
	int				bytesRead = 0;
    int             i;
    int             delimPos = 0;
	const char *	delimiters = "\n\r";

	fptr = fopen(szConfigFileName, "rt");

	if (fptr == NULL) {
		log.logError("Failed to open config file %s", szConfigFileName);
		throw new Exception("ERROR reading config");
	}

    fseek(fptr, 0L, SEEK_END);
    fileLength = ftell(fptr);
    rewind(fptr);

	config = (char *)malloc(fileLength);

	if (config == NULL) {
		log.logError("Failed to alocate %d bytes for config file %s", fileLength, szConfigFileName);
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
		log.logError("Read %d bytes, but config file %s is %d bytes long", bytesRead, szConfigFileName, fileLength);
		throw new Exception("Failed to read in config file.");
	}

	fclose(fptr);

	log.logInfo("Read %d bytes from config file %s", bytesRead, szConfigFileName);

	pszConfigLine = strtok_r(config, delimiters, &reference);

	while (pszConfigLine != NULL) {
        if (pszConfigLine[0] == '#') {
            /*
            ** Ignore line comments...
            */
            pszConfigLine = strtok_r(NULL, delimiters, &reference);
            continue;
        }

        log.logDebug("Read %d chars of line '%s'", strlen(pszConfigLine), pszConfigLine);

        /*
        ** Try and cope with weird error (seen on mac os when testing) where strtok()
        ** returns some random chars if there is a new line at the end of the file...
        */
        if (strlen(pszConfigLine) > 0 || strstr(pszConfigLine, delimiters) != NULL) {
            for (i = 0;i < (int)strlen(pszConfigLine);i++) {
                if (pszConfigLine[i] == '=') {
                    pszKey = strndup(pszConfigLine, i);
                    delimPos = i;
                }
                if (delimPos) {
                    if (pszConfigLine[i] == '#' || pszConfigLine[i] == ' ') {
                        pszValue = strndup(&pszConfigLine[delimPos + 1], i - delimPos);
                        break;
                    }
                    else {
                        pszValue = strndup(&pszConfigLine[delimPos + 1], strlen(pszConfigLine) - delimPos);
                        break;
                    }
                }
            }

            delimPos = 0;

            log.logDebug("Got key '%s' and value '%s'", pszKey, pszValue);

            values[string(pszKey)] = string(pszValue);

            free(pszKey);
            free(pszValue);
        }

        pszConfigLine = strtok_r(NULL, delimiters, &reference);
	}

	free(config);

    isConfigured = true;
}

void ConfigManager::getCFG()
{
    values[string("web.host")] = string("peaceful-earth-94757.herokuapp.com");
    values[string("web.port")] = string("443");
    values[string("web.issecure")] = string("yes");
    values[string("web.basepath")] = string("/weather/api/");
    values[string("admin.listenport")] = string("80");
    values[string("admin.docroot")] = string("/var/www/html");
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
