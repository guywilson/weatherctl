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

    strcpy(_dbTableName, cfg.getValueAsCstr("calibration.dbtable"));

    retrieve();
}

void CalibrationData::retrieve()
{
    char        szRowName[32];
    int16_t     offset;
    double      factor;
    int         i;

    Postgres pg(
        cfg.getValueAsCstr("calibration.dbhost"), 
        5432, 
        cfg.getValueAsCstr("calibration.dbname"), 
        "guy", 
        "password");

    for (i = 0;i < NUM_CALIBRATION_PAIRS;i++) {
        strcpy(szRowName, &_dbRowNames[i][0]);

        pg.getCalibrationData(szRowName, &offset, &factor);

        setOffset((SensorType)i, offset);
        setFactor((SensorType)i, factor);
    }
}

void CalibrationData::save()
{
    char            szUpdateStatement[512];
    int             i;

    Postgres pg(
        cfg.getValueAsCstr("calibration.dbhost"), 
        5432, 
        cfg.getValueAsCstr("calibration.dbname"), 
        "guy", 
        "password");

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
}

int16_t CalibrationData::getOffset(SensorType t)
{
    return _offsets[t];
}
char * CalibrationData::getOffsetAsCStr(SensorType t)
{
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

double CalibrationData::getFactor(SensorType t)
{
    return _factors[t];
}
char * CalibrationData::getFactorAsCStr(SensorType t)
{
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
