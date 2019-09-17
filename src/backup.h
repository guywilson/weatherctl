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
    char *          pszCSVFileName;
    char *          pszPrimaryDBHost;
    char *          pszPrimaryDBName;
    char *          pszSecondaryDBHost;
    char *          pszSecondaryDBName;

    vector<string>  csvHeader = {"TIME", "TYPE", "TEMPERATURE", "PRESSURE", "HUMIDITY"};

    FILE *          fptr_csv = NULL;
    PGconn *		dbConnection = NULL;
    int             numColumns = csvHeader.size();
    Logger &        log = Logger::getInstance();

    BackupManager() {}
    void            writeCSVHeader();
    void            writeCSVRecord(PostDataTPH * pPostData);
    void            writeDBRecord(const char * pszHost, const char * pszDbName, PostDataTPH * pPostData);

public:
    ~BackupManager();

    void            close();
    
    void            setupCSV(const char * pszFilename);
    void            setupPrimaryDB(const char * pszHostname, const char * pszDBName);
    void            setupSecondaryDB(const char * pszHostname, const char * pszDBName);

    uint16_t        backup(PostDataTPH * pPostData);
};

#endif