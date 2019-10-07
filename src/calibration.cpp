#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calibration.h"
#include "configmgr.h"
#include "postgres.h"
#include "logger.h"

using namespace std;

CalibrationData::CalibrationData()
{
    memset(_strOffsets, 0, (NUM_CALIBRATION_PAIRS * STR_VALUE_LENGTH));
    memset(_strFactors, 0, (NUM_CALIBRATION_PAIRS * STR_VALUE_LENGTH));

    strcpy(_dbTableName, cfg.getValue("calibration.dbtable"));

    retrieve();
}

void CalibrationData::retrieve()
{
    char        szSelectStatement[256];
    int         i;

    Postgres pg(
        cfg.getValue("calibration.dbhost"), 
        5432, 
        cfg.getValue("calibration.dbname"), 
        "guy", 
        "password");

    sprintf(
        szSelectStatement, 
        "SELECT name, offset_amount, factor from %s", 
        cfg.getValue("calibration.dbtable"));

    PGResult result = pg.SELECT(szSelectStatement);

    for (i = 0;i < result.getNumRows();i++) {
        PGRow row = result.getRow(i);

        PGField name = row.getField(string("name"));

        if (strcmp(name.getValue(), "thermometer") == 0) {
            PGField offset = row.getField(string("offset_amount"));
            PGField factor = row.getField(string("factor"));

            setOffset(thermometer, offset.getValue());
            setFactor(thermometer, factor.getValue());
        }
        else if (strcmp(name.getValue(), "barometer") == 0) {
            PGField offset = row.getField(string("offset_amount"));
            PGField factor = row.getField(string("factor"));

            setOffset(barometer, offset.getValue());
            setFactor(barometer, factor.getValue());
        }
        else if (strcmp(name.getValue(), "hygrometer") == 0) {
            PGField offset = row.getField(string("offset_amount"));
            PGField factor = row.getField(string("factor"));

            setOffset(hygrometer, offset.getValue());
            setFactor(hygrometer, factor.getValue());
        }
        else if (strcmp(name.getValue(), "anemometer") == 0) {
            PGField offset = row.getField(string("offset_amount"));
            PGField factor = row.getField(string("factor"));

            setOffset(anemometer, offset.getValue());
            setFactor(anemometer, factor.getValue());
        }
        else if (strcmp(name.getValue(), "rainGauge") == 0) {
            PGField offset = row.getField(string("offset_amount"));
            PGField factor = row.getField(string("factor"));

            setOffset(rainGauge, offset.getValue());
            setFactor(rainGauge, factor.getValue());
        }
    }

    isStale = false;
}

void CalibrationData::save()
{
    char            szUpdateStatement[512];
    int             i;

    Postgres pg(
        cfg.getValue("calibration.dbhost"), 
        5432, 
        cfg.getValue("calibration.dbname"), 
        "guy", 
        "password");

    pg.beginTransaction();

    for (i = 0;i < NUM_CALIBRATION_PAIRS;i++) {
        sprintf(
            szUpdateStatement, 
            "UPDATE %s SET OFFSET_AMOUNT = '%d', FACTOR = '%.3f' WHERE NAME = '%s'",
            _dbTableName,
            getOffset((SensorType)i),
            getFactor((SensorType)i),
            &_dbRowNames[i][0]);

        pg.UPDATE(szUpdateStatement);
    }

    pg.endTransaction();

    isStale = true;
}

int16_t CalibrationData::getOffset(SensorType t)
{
    if (isStale) {
        retrieve();
    }

    return _offsets[t];
}
char * CalibrationData::getOffsetAsCStr(SensorType t)
{
    if (isStale) {
        retrieve();
    }
    
    return &_strOffsets[t][0];
}
void CalibrationData::setOffset(SensorType t, int16_t offset)
{
    _offsets[t] = offset;

    sprintf(&_strOffsets[t][0], "%d", offset);
}
void CalibrationData::setOffset(SensorType t, char * pszOffset)
{
    setOffset(t, atoi(pszOffset));
}
void CalibrationData::setOffset(SensorType t, const char * pszOffset)
{
    strcpy(&_strOffsets[t][0], pszOffset);
    setOffset(t, &_strOffsets[t][0]);
}

double CalibrationData::getFactor(SensorType t)
{
    if (isStale) {
        retrieve();
    }
    
    return _factors[t];
}
char * CalibrationData::getFactorAsCStr(SensorType t)
{
    if (isStale) {
        retrieve();
    }
    
    return &_strFactors[t][0];
}
void CalibrationData::setFactor(SensorType t, double factor)
{
    _factors[t] = factor;

    sprintf(&_strFactors[t][0], "%.3f", factor);
}
void CalibrationData::setFactor(SensorType t, char * pszFactor)
{
    setFactor(t, atof(pszFactor));
}
void CalibrationData::setFactor(SensorType t, const char * pszFactor)
{
    strcpy(&_strFactors[t][0], pszFactor);
    setFactor(t, &_strFactors[t][0]);
}
