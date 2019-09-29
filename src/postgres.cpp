#include <iostream>
#include <vector>
#include <map>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <libpq-fe.h>
#else
#include <postgresql/libpq-fe.h>
#endif

#include "postgres.h"
#include "exception.h"
#include "logger.h"

using namespace std;

PGField::PGField(char * name, char * value)
{

}

PGField::~PGField()
{

}

bool PGField::isNull()
{
    return true;
}

string PGField::getName()
{
    return "";
}

string PGField::getValue()
{
    return "";
}

const char * PGField::getValueAsCStr()
{
    return getValue().c_str();
}

PGRow::PGRow()
{

}

void PGRow::addField(PGField & field)
{

}

PGResult::PGResult(PGresult * results)
{

}

PGResult::~PGResult()
{

}

int PGResult::getNumRows()
{
    return 0;
}

PGRow PGResult::getRow(int index)
{
    PGRow r;
    return r;
}

PGRow PGResult::getFirst()
{
    return getRow(0);
}

Postgres::Postgres(const char * host, int port, const char * dbName, const char * user, const char * password)
{
    char            szConnection[256];

    sprintf(
        szConnection, 
        "host=%s port=%d dbname=%s user=%s password=%s", 
        host,
        port,
        dbName,
        user,
        password);

    dbConnection = PQconnectdb(szConnection);

    if (PQstatus(dbConnection) != CONNECTION_OK) {
        log.logError("Cannot connect to database [%s]", PQerrorMessage(dbConnection));
        PQfinish(dbConnection);
        throw new Exception("Cannot connect to database");
    }
}

Postgres::~Postgres()
{
    if (dbConnection != NULL) {
        PQfinish(dbConnection);
    }
}

PGresult * Postgres::_execute(const char * sql)
{
	PGresult *			queryResult;

    queryResult = PQexec(dbConnection, "BEGIN");

    if (PQresultStatus(queryResult) != PGRES_COMMAND_OK) {
        log.logError("Error beginning transaction [%s]", PQerrorMessage(dbConnection));
        PQclear(queryResult);
        PQfinish(dbConnection);
        throw new Exception("Error opening transaction");
    }

    log.logDebug("Opened DB transaction");

    PQclear(queryResult);

    queryResult = PQexec(dbConnection, sql);

    if (PQresultStatus(queryResult) != PGRES_COMMAND_OK && PQresultStatus(queryResult) != PGRES_TUPLES_OK) {
        log.logError("Error issuing statement [%s]: '%s'", sql, PQerrorMessage(dbConnection));
        PQclear(queryResult);
        PQfinish(dbConnection);
        throw new Exception("Error issuing statement");
    }
    else {
        log.logInfo("Successfully executed statement [%s]", sql);
    }

    PQexec(dbConnection, "END");

    log.logDebug("Closed DB transaction");

    return queryResult;
}

void Postgres::getCalibrationData(PCALIBRATION_DATA data)
{
    PGresult *                      r;
    int                             rows = 0;
    int                             row;
    char *                          name;

    r = _execute("select id, name, offset_amount, factor from calibration");

    rows = PQntuples(r);

    for (row = 0;row < rows;row++) {
        name = PQgetvalue(r, row, 1);

        log.logDebug("Got calibration values for '%s'", name);

        if (strcmp(name, "thermometer") == 0) {
            data->thermometerOffset = atoi(PQgetvalue(r, row, 2));
            data->thermometerFactor = atof(PQgetvalue(r, row, 3));

            log.logDebug("Got offset [%d] factor [%.3f] '%s'", data->thermometerOffset, data->thermometerFactor, name);
        }
        else if (strcmp(name, "barometer") == 0) {
            data->barometerOffset = atoi(PQgetvalue(r, row, 2));
            data->barometerFactor = atof(PQgetvalue(r, row, 3));

            log.logDebug("Got offset [%d] factor [%.3f] '%s'", data->barometerOffset, data->barometerFactor, name);
        }
        else if (strcmp(name, "humidity") == 0) {
            data->humidityOffset = atoi(PQgetvalue(r, row, 2));
            data->humidityFactor = atof(PQgetvalue(r, row, 3));

            log.logDebug("Got offset [%d] factor [%.3f] '%s'", data->humidityOffset, data->humidityFactor, name);
        }
        else if (strcmp(name, "anemometer") == 0) {
            data->anemometerOffset = atoi(PQgetvalue(r, row, 2));
            data->anemometerFactor = atof(PQgetvalue(r, row, 3));

            log.logDebug("Got offset [%d] factor [%.3f] '%s'", data->anemometerOffset, data->anemometerFactor, name);
        }
        else if (strcmp(name, "rain") == 0) {
            data->rainGaugeOffset = atoi(PQgetvalue(r, row, 2));
            data->rainGaugeFactor = atof(PQgetvalue(r, row, 3));

            log.logDebug("Got offset [%d] factor [%.3f] '%s'", data->rainGaugeOffset, data->rainGaugeFactor, name);
        }
    }
}

PGResult * Postgres::SELECT(const char * sql)
{
    PGResult *                      pResults;
    PGresult *                      r;
    int                             rows = 0;
    int                             columns = 0;
    int                             row;
    int                             col;

    pResults = new PGResult(_execute(sql));

    rows = PQntuples(r);
    columns = PQnfields(r);

    for (row = 0;row < rows;row++) {
        for (col = 0;col < columns;col++) {
        }
    }

    return pResults;
}

PGResult * Postgres::find(const char * sql)
{
    PGResult *                      pResults;

    pResults = SELECT(sql);

    return pResults;
}

int Postgres::INSERT(const char * sql)
{
    PGresult *      r;
    int             numRowsAffected = 0;

    r = _execute(sql);

    numRowsAffected = atoi(PQcmdTuples(r));

    return numRowsAffected;
}

int Postgres::UPDATE(const char * sql)
{
    PGresult *      r;
    int             numRowsAffected = 0;

    r = _execute(sql);

    numRowsAffected = atoi(PQcmdTuples(r));

    return numRowsAffected;
}

int Postgres::DELETE(const char * sql)
{
    PGresult *      r;
    int             numRowsAffected = 0;

    r = _execute(sql);

    numRowsAffected = atoi(PQcmdTuples(r));

    return numRowsAffected;
}
