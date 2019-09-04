#include <iostream>
#include <array>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <libpq-fe.h>
#else
#include <postgresql/libpq-fe.h>
#endif

#include "exception.h"
#include "webconnect.h"
#include "backup.h"
#include "logger.h"

using namespace std;

BackupManager::~BackupManager()
{
    this->close();
}

void BackupManager::close()
{
    fclose(fptr_csv);
    PQfinish(dbConnection);
}

void BackupManager::setupCSV(const char * pszFilename)
{
    this->pszCSVFileName = strdup(pszFilename);
}

void BackupManager::setupPrimaryDB(const char * pszHostname, const char * pszDBName)
{
    this->pszPrimaryDBHost = strdup(pszHostname);
    this->pszPrimaryDBName = strdup(pszDBName);
}

void BackupManager::setupSecondaryDB(const char * pszHostname, const char * pszDBName)
{
    this->pszSecondaryDBHost = strdup(pszHostname);
    this->pszSecondaryDBName = strdup(pszDBName);
}

void BackupManager::writeCSVHeader()
{
    int         i;

    for (i = 0;i < numColumns;i++) {
        fputs(csvHeader.at(i).c_str(), fptr_csv);

        if (i < (numColumns - 1)) {
            fputc(',', fptr_csv);
        }
    }

    fputc('\n', fptr_csv);

    fflush(fptr_csv);
}

void BackupManager::writeCSVRecord(PostData * pPostData)
{
    FILE *      f;
    bool        isNewFile = false;

    if (this->fptr_csv == NULL) {
        f = fopen(this->pszCSVFileName, "rt");

        if (f == NULL) {
            isNewFile = true;
        }
        else {
            fclose(f);
        }

        this->fptr_csv = fopen(this->pszCSVFileName, "at");

        if (this->fptr_csv == NULL) {
            log.logError("Failed to open CSV file: %s", strerror(errno));
            throw new Exception("Error opening CSV file");
        }

        if (isNewFile) {
            writeCSVHeader();
        }
    }

    fputs(pPostData->getTimestamp(), fptr_csv);
    fputc(',', fptr_csv);
    fputs(pPostData->getType(), fptr_csv);
    fputc(',', fptr_csv);
    fputs(pPostData->getTemperature(), fptr_csv);
    fputc(',', fptr_csv);
    fputs(pPostData->getPressure(), fptr_csv);
    fputc(',', fptr_csv);
    fputs(pPostData->getHumidity(), fptr_csv);
    fputc('\n', fptr_csv);

    fflush(fptr_csv);
}

void BackupManager::writeDBRecord(const char * pszHost, const char * pszDbName, PostData * pPostData)
{
	char				szConnection[128];
	PGresult *			queryResult;
	const char *		pszInsertTemplate;
	char				szInsertStr[128];

    sprintf(
        szConnection, 
        "host=%s port=5432 dbname=%s user=guy password=password", 
        pszHost, 
        pszDbName);

    dbConnection = PQconnectdb(szConnection);

    if (PQstatus(dbConnection) != CONNECTION_OK) {
        log.logError("Cannot connect to database [%s]", PQerrorMessage(dbConnection));
        PQfinish(dbConnection);
        throw new Exception("Cannot connect to database");
    }

    queryResult = PQexec(dbConnection, "BEGIN");

    if (PQresultStatus(queryResult) != PGRES_COMMAND_OK) {
        log.logError("Error beginning transaction [%s]", PQerrorMessage(dbConnection));
        PQclear(queryResult);
        PQfinish(dbConnection);
        throw new Exception("Error opening transaction");
    }

    log.logDebug("Opened DB transaction");

    PQclear(queryResult);

    CurrentTime time;

    pszInsertTemplate= "INSERT INTO TPH (TS, TYPE, TEMPERATURE, PRESSURE, HUMIDITY) VALUES ('%s', '%s', %s, %s, %s)";

    sprintf(
        szInsertStr, 
        pszInsertTemplate, 
        time.getTimeStamp(), 
        pPostData->getType(), 
        pPostData->getTemperature(), 
        pPostData->getPressure(), 
        pPostData->getHumidity());

    queryResult = PQexec(dbConnection, szInsertStr);

    if (PQresultStatus(queryResult) != PGRES_COMMAND_OK) {
        log.logError("Error issuing INSERT statement [%s]", PQerrorMessage(dbConnection));
        PQclear(queryResult);
        PQfinish(dbConnection);
        throw new Exception("Error issuing INSERT statement");
    }
    else {
        log.logInfo("Successfully INSERTed record to database.");
    }

    PQclear(queryResult);

    queryResult = PQexec(dbConnection, "END");
    PQclear(queryResult);

    log.logDebug("Closed DB transaction");

    PQfinish(dbConnection);

    log.logDebug("PQfinish()");
}

uint16_t BackupManager::backup(PostData * pPostData)
{
    uint16_t            rtn = BACKUP_NOT_REQUIRED_SKIPPED;

    /*
    ** Only save a backup if we tried to tell the
    ** web server to save...
    */
    if (pPostData->isDoSave()) {
        try {
            writeDBRecord(this->pszPrimaryDBHost, this->pszPrimaryDBName, pPostData);

            log.logInfo("Written backup record to primary DB %s on %s", this->pszPrimaryDBName, this->pszPrimaryDBHost);

            rtn = BACKUP_COMPLETE_PRIMARY_DB;
        }
        catch (Exception * e) {
            log.logError("Failed to insert to primary database, maybe network error?");
            log.logInfo("Insert to secndary DB instance instead, you will need to reconcile later...");

            try {
                writeDBRecord(this->pszSecondaryDBHost, this->pszSecondaryDBName, pPostData);

                log.logInfo("Written backup record to secondary DB %s on %s", this->pszSecondaryDBName, this->pszSecondaryDBHost);

                rtn = BACKUP_COMPLETE_SECONDARY_DB;
            }
            catch (Exception * e2) {
                log.logError("Failed to insert to secondary database, check your configuration!");
                log.logInfo("Writing to CSV file %s, you will need to reconcile later...", this->pszCSVFileName);

                try {
                    writeCSVRecord(pPostData);

                    rtn = BACKUP_COMPLETE_CSV;
                }
                catch (Exception * e3) {
                    log.logError("Failed to write to CSV file %s, out of options!!", this->pszCSVFileName);
                    log.logInfo("Data will be lost!");

                    rtn = BACKUP_FAILED_DATA_LOST;
                }
            }
        }
    }

    return rtn;
}