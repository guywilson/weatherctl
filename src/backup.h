#include <array>
#include <vector>
#include <stdio.h>
#include <stdint.h>

#ifdef __APPLE__
#include <libpq-fe.h>
#else
#include <postgresql/libpq-fe.h>
#endif

#include "webadmin.h"
#include "logger.h"

using namespace std;

#ifndef _INCL_BACKUP
#define _INCL_BACKUP

#define BACKUP_COMPLETE_CSV             0x0100
#define BACKUP_COMPLETE_PRIMARY_DB      0x0200
#define BACKUP_COMPLETE_SECONDARY_DB    0x0400
#define BACKUP_FAILED_DATA_LOST         0x0000
#define BACKUP_NOT_REQUIRED_SKIPPED     0x0001

class BackupManager
{
public:
	static BackupManager & getInstance()
	{
		static BackupManager instance;
		return instance;
	}

private:
    char *          pszTphCSVFileName;
    char *          pszWindCSVFileName;
    char *          pszRainCSVFileName;
    char *          pszPrimaryDBHost;
    char *          pszPrimaryDBName;
    char *          pszSecondaryDBHost;
    char *          pszSecondaryDBName;

    char            szDefaultCSVFileName[256];

    const char *    csvHeaderTPH[5] = {"TIME", "TYPE", "TEMPERATURE", "PRESSURE", "HUMIDITY"};
    const char *    csvHeaderWind[3] = {"TIME", "TYPE", "WINDSPEED"};
    const char *    csvHeaderRain[3] = {"TIME", "TYPE", "RAINFALL"};
    const char *    csvHeaderDefault[3] = {"TIME", "TYPE", "ERROR"};

    FILE *          fptr_tph = NULL;
    FILE *          fptr_wind = NULL;
    FILE *          fptr_rain = NULL;
    FILE *          fptr_default = NULL;
    Logger &        log = Logger::getInstance();

    BackupManager() {}

    bool            isDoSave(PostData * pPostData);
    FILE *          openCSV(PostData * pPostData);
    void            writeCSVHeader(PostData * pPostData);
    void            writeCSVRecord(PostData * pPostData);
    void            writeDBRecord(const char * pszHost, const char * pszDbName, PostData * pPostData);

public:
    ~BackupManager();

    void            close();
    
    void            setupCSV(const char * pszTphFilename, const char * pszWindFilename, const char * pszRainFilename);
    void            setupPrimaryDB(const char * pszHostname, const char * pszDBName);
    void            setupSecondaryDB(const char * pszHostname, const char * pszDBName);

    uint16_t        backup(PostData * pPostData);
};

#endif