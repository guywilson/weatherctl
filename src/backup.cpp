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

#include "wctl_error.h"
#include "postgres.h"
#include "postdata.h"
#include "backup.h"
#include "logger.h"

using namespace std;

BackupManager::~BackupManager()
{
    this->close();
}

void BackupManager::close()
{
    if (fptr_tph != NULL) {
        fclose(fptr_tph);
    }
    if (fptr_wind != NULL) {
        fclose(fptr_wind);
    }
    if (fptr_rain != NULL) {
        fclose(fptr_rain);
    }
}

void BackupManager::setupCSV(const char * pszTphFilename, const char * pszWindFilename, const char * pszRainFilename)
{
    this->pszTphCSVFileName = strdup(pszTphFilename);
    this->pszWindCSVFileName = strdup(pszWindFilename);
    this->pszRainCSVFileName = strdup(pszRainFilename);

    strcpy(this->szDefaultCSVFileName, "/usr/local/bin/default.csv");
}

void BackupManager::setupPrimaryDB(const char * pszHostname, const char * pszDBName)
{
    this->pszPrimaryDBHost = strdup(pszHostname);
    this->pszPrimaryDBName = strdup(pszDBName);

    if (strlen(this->pszPrimaryDBHost) > 0 && strlen(this->pszPrimaryDBName) > 0) {
        this->primarySourceConfigured = true;
    }
}

void BackupManager::setupSecondaryDB(const char * pszHostname, const char * pszDBName)
{
    this->pszSecondaryDBHost = strdup(pszHostname);
    this->pszSecondaryDBName = strdup(pszDBName);

    if (strlen(this->pszSecondaryDBHost) > 0 && strlen(this->pszSecondaryDBName) > 0) {
        this->secondarySourceConfigured = true;
    }
}

bool BackupManager::isDoSave(PostData * pPostData)
{
    bool            isDoSave = false;

    switch (pPostData->getClassID()) {
        case CLASS_ID_TPH:
            isDoSave = ((PostDataTPH *)pPostData)->isDoSave();
            break;

        case CLASS_ID_WINDSPEED:
            isDoSave = (((PostDataWindspeed *)pPostData)->isDoSaveAvg() || ((PostDataWindspeed *)pPostData)->isDoSaveMax());
            break;

        case CLASS_ID_RAINFALL:
            isDoSave = (((PostDataRainfall *)pPostData)->isDoSaveAvg() || ((PostDataRainfall *)pPostData)->isDoSaveTotal());
            break;
    }

    return isDoSave;
}

FILE * BackupManager::openCSV(PostData * pPostData)
{
    FILE *      f;
    FILE *      fptr_csv;
    bool        isNewFile = false;
    char *      pszCSVFileName;

    switch (pPostData->getClassID()) {
        case CLASS_ID_TPH:
            fptr_csv = fptr_tph;
            pszCSVFileName = this->pszTphCSVFileName;
            break;

        case CLASS_ID_WINDSPEED:
            fptr_csv = fptr_wind;
            pszCSVFileName = this->pszWindCSVFileName;
            break;

        case CLASS_ID_RAINFALL:
            fptr_csv = fptr_rain;
            pszCSVFileName = this->pszRainCSVFileName;
            break;

        default:
            fptr_csv = fptr_default;
            pszCSVFileName = this->szDefaultCSVFileName;
            break;
    }

    log.logInfo("Writing to CSV file %s, you will need to reconcile later...", pszCSVFileName);

    if (fptr_csv == NULL) {
        f = fopen(pszCSVFileName, "rt");

        if (f == NULL) {
            isNewFile = true;
        }
        else {
            fclose(f);
        }

        fptr_csv = fopen(pszCSVFileName, "at");

        if (fptr_csv == NULL) {
            log.logError("Failed to open CSV file %s with error %s", pszCSVFileName, strerror(errno));
            throw wctl_error(wctl_error::buildMsg("Error opening CSV file %s with error %s", pszCSVFileName, strerror(errno)), __FILE__, __LINE__);
        }

        switch (pPostData->getClassID()) {
            case CLASS_ID_TPH:
                this->fptr_tph = fptr_csv;
                break;

            case CLASS_ID_WINDSPEED:
                this->fptr_wind = fptr_csv;
                break;

            case CLASS_ID_RAINFALL:
                this->fptr_rain = fptr_csv;
                break;

            default:
                this->fptr_default = fptr_csv;
                break;
        }

        if (isNewFile) {
            writeCSVHeader(pPostData);
        }
    }

    return fptr_csv;
}

void BackupManager::writeCSVHeader(PostData * pPostData)
{
    int             i;
    int             numColumns = 0;
    FILE *          fptr_csv = NULL;
    const char **   csvHeader;

    switch (pPostData->getClassID()) {
        case CLASS_ID_TPH:
            numColumns = 5;
            csvHeader = csvHeaderTPH;
            fptr_csv = this->fptr_tph;
            break;

        case CLASS_ID_WINDSPEED:
            numColumns = 3;
            csvHeader = csvHeaderWind;
            fptr_csv = this->fptr_wind;
            break;

        case CLASS_ID_RAINFALL:
            numColumns = 3;
            csvHeader = csvHeaderRain;
            fptr_csv = this->fptr_rain;
            break;

        default:
            numColumns = 3;
            csvHeader = csvHeaderDefault;
            fptr_csv = this->fptr_default;
            break;
    }

    if (fptr_csv != NULL) {
        for (i = 0;i < numColumns;i++) {
            fputs(csvHeader[i], fptr_csv);

            if (i < (numColumns - 1)) {
                fputc(',', fptr_csv);
            }
        }

        fputc('\n', fptr_csv);

        fflush(fptr_csv);
    }
}

void BackupManager::writeCSVRecord(PostData * pPostData)
{
    FILE *              fptr_csv;
    PostDataTPH *       pPostDataTPH;
    PostDataWindspeed * pPostDataWind;
    PostDataRainfall *  pPostDataRain;

    switch (pPostData->getClassID()) {
        case CLASS_ID_TPH:
            pPostDataTPH = (PostDataTPH *)pPostData;

            if (pPostDataTPH->isDoSave()) {
                fptr_csv = openCSV(pPostData);

                fputs(pPostDataTPH->getTimestamp(), fptr_csv);
                fputc(',', fptr_csv);
                fputs(pPostDataTPH->getType(), fptr_csv);
                fputc(',', fptr_csv);
                fputs(pPostDataTPH->getTemperature(), fptr_csv);
                fputc(',', fptr_csv);
                fputs(pPostDataTPH->getPressure(), fptr_csv);
                fputc(',', fptr_csv);
                fputs(pPostDataTPH->getHumidity(), fptr_csv);
                fputc('\n', fptr_csv);
                fflush(fptr_csv);
            }
            break;

        case CLASS_ID_WINDSPEED:
            pPostDataWind = (PostDataWindspeed *)pPostData;

            if (pPostDataWind->isDoSaveAvg()) {
                fptr_csv = openCSV(pPostData);
                
                fputs(pPostDataWind->getTimestamp(), fptr_csv);
                fputc(',', fptr_csv);
                fputs("AVG", fptr_csv);
                fputc(',', fptr_csv);
                fputs(pPostDataWind->getAvgWindspeed(), fptr_csv);
                fputc('\n', fptr_csv);
                fflush(fptr_csv);
            }
            if (pPostDataWind->isDoSaveMax()) {
                fptr_csv = openCSV(pPostData);
                
                fputs(pPostDataWind->getTimestamp(), fptr_csv);
                fputc(',', fptr_csv);
                fputs("MAX", fptr_csv);
                fputc(',', fptr_csv);
                fputs(pPostDataWind->getMaxWindspeed(), fptr_csv);
                fputc('\n', fptr_csv);
                fflush(fptr_csv);
            }
            break;

        case CLASS_ID_RAINFALL:
            pPostDataRain = (PostDataRainfall *)pPostData;

            if (pPostDataRain->isDoSaveAvg()) {
                fptr_csv = openCSV(pPostData);
                
                fputs(pPostDataRain->getTimestamp(), fptr_csv);
                fputc(',', fptr_csv);
                fputs("AVG", fptr_csv);
                fputc(',', fptr_csv);
                fputs(pPostDataRain->getAvgRainfall(), fptr_csv);
                fputc('\n', fptr_csv);
                fflush(fptr_csv);
            }
            if (pPostDataRain->isDoSaveTotal()) {
                fptr_csv = openCSV(pPostData);
                
                fputs(pPostDataRain->getTimestamp(), fptr_csv);
                fputc(',', fptr_csv);
                fputs("TOT", fptr_csv);
                fputc(',', fptr_csv);
                fputs(pPostDataRain->getTotalRainfall(), fptr_csv);
                fputc('\n', fptr_csv);
                fflush(fptr_csv);
            }
            break;

        default:
            fptr_csv = openCSV(pPostData);
            
            fputs(pPostData->getTimestamp(), fptr_default);
            fputc(',', fptr_default);
            fputs("BASE", fptr_default);
            fputc(',', fptr_default);
            fputs("ERROR - Unknown type", fptr_default);
            fputc('\n', fptr_default);
            fflush(fptr_default);
            break;
    }
}

void BackupManager::writeDBRecord(const char * pszHost, const char * pszDbName, PostData * pPostData)
{
    Postgres            pg(pszHost, 5432, pszDbName, "guy", "password");
    PostDataTPH *       pPostDataTPH;
    PostDataWindspeed * pPostDataWind;
    PostDataRainfall *  pPostDataRain;
	const char *		pszInsertTemplate;
	char				szInsertStr[256];

    CurrentTime time;

    switch (pPostData->getClassID()) {
        case CLASS_ID_TPH:
            pPostDataTPH = (PostDataTPH *)pPostData;

            pszInsertTemplate= "INSERT INTO TPH (TS, TYPE, TEMPERATURE, PRESSURE, HUMIDITY) VALUES ('%s', '%s', %s, %s, %s)";

            if (pPostDataTPH->isDoSave()) {
                sprintf(
                    szInsertStr, 
                    pszInsertTemplate, 
                    time.getTimeStamp(), 
                    pPostDataTPH->getType(), 
                    pPostDataTPH->getTemperature(), 
                    pPostDataTPH->getPressure(), 
                    pPostDataTPH->getHumidity());
                
                try {
                    pg.INSERT(szInsertStr);
                }
                catch (wctl_error & e) {
                    log.logError("Error inserting backup record...");
                    throw wctl_error(wctl_error::buildMsg("Error inserting backup record... [%s]", e.what()), __FILE__, __LINE__);
                }
            }
            break;

        case CLASS_ID_WINDSPEED:
            pPostDataWind = (PostDataWindspeed *)pPostData;

            pszInsertTemplate= "INSERT INTO WIND (TS, TYPE, WINDSPEED) VALUES ('%s', '%s', %s)";

            if (pPostDataWind->isDoSaveAvg()) {
                sprintf(
                    szInsertStr, 
                    pszInsertTemplate, 
                    time.getTimeStamp(), 
                    "AVG", 
                    pPostDataWind->getAvgWindspeed());
            
                try {
                    pg.INSERT(szInsertStr);
                }
                catch (wctl_error & e) {
                    log.logError("Error inserting backup record...");
                    throw wctl_error(wctl_error::buildMsg("Error inserting backup record... [%s]", e.what()), __FILE__, __LINE__);
                }
            }
            if (pPostDataWind->isDoSaveMax()) {
                sprintf(
                    szInsertStr, 
                    pszInsertTemplate, 
                    time.getTimeStamp(), 
                    "MAX", 
                    pPostDataWind->getMaxWindspeed());
            
                try {
                    pg.INSERT(szInsertStr);
                }
                catch (wctl_error & e) {
                    log.logError("Error inserting backup record...");
                    throw wctl_error(wctl_error::buildMsg("Error inserting backup record... [%s]", e.what()), __FILE__, __LINE__);
                }
            }
            break;

        case CLASS_ID_RAINFALL:
            pPostDataRain = (PostDataRainfall *)pPostData;

            pszInsertTemplate= "INSERT INTO RAIN (TS, TYPE, RAINFALL) VALUES ('%s', '%s', %s)";

            if (pPostDataRain->isDoSaveAvg()) {
                sprintf(
                    szInsertStr, 
                    pszInsertTemplate, 
                    time.getTimeStamp(), 
                    "AVG", 
                    pPostDataRain->getAvgRainfall());
            
                try {
                    pg.INSERT(szInsertStr);
                }
                catch (wctl_error & e) {
                    log.logError("Error inserting backup record...");
                    throw wctl_error(wctl_error::buildMsg("Error inserting backup record... [%s]", e.what()), __FILE__, __LINE__);
                }
            }
            if (pPostDataRain->isDoSaveTotal()) {
                sprintf(
                    szInsertStr, 
                    pszInsertTemplate, 
                    time.getTimeStamp(), 
                    "TOT", 
                    pPostDataRain->getTotalRainfall());
            
                try {
                    pg.INSERT(szInsertStr);
                }
                catch (wctl_error & e) {
                    log.logError("Error inserting backup record...");
                    throw wctl_error(wctl_error::buildMsg("Error inserting backup record... [%s]", e.what()), __FILE__, __LINE__);
                }
            }
            break;
    }
}

uint16_t BackupManager::backup(PostData * pPostData)
{
    uint16_t            rtn = BACKUP_NOT_REQUIRED_SKIPPED;

    if (this->isDoSave(pPostData)) {
		log.logInfo("Attempting to backup data");

        try {
            if (this->primarySourceConfigured) {
                writeDBRecord(this->pszPrimaryDBHost, this->pszPrimaryDBName, pPostData);

                log.logInfo("Written backup record to primary DB %s on %s", this->pszPrimaryDBName, this->pszPrimaryDBHost);

                rtn = BACKUP_COMPLETE_PRIMARY_DB;
            }
        }
        catch (wctl_error & e) {
            log.logError("Failed to insert to primary database, maybe network error?");
            log.logInfo("Insert to secndary DB instance instead, you will need to reconcile later...");

            try {
                if (this->secondarySourceConfigured) {
                    writeDBRecord(this->pszSecondaryDBHost, this->pszSecondaryDBName, pPostData);

                    log.logInfo("Written backup record to secondary DB %s on %s", this->pszSecondaryDBName, this->pszSecondaryDBHost);

                    rtn = BACKUP_COMPLETE_SECONDARY_DB;
                }
            }
            catch (wctl_error & e2) {
                log.logError("Failed to insert to secondary database, check your configuration!");

                try {
                    writeCSVRecord(pPostData);

                    rtn = BACKUP_COMPLETE_CSV;
                }
                catch (wctl_error & e3) {
                    log.logError("Failed to write to CSV file, out of options!!");
                    log.logInfo("Data will be lost!");

                    rtn = BACKUP_FAILED_DATA_LOST;
                }
            }
        }
    }

    return rtn;
}