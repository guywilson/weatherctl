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
#include "configmgr.h"
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

void Postgres::beginTransaction()
{
	PGresult *			queryResult;
    
    queryResult = PQexec(dbConnection, "BEGIN");

    if (PQresultStatus(queryResult) != PGRES_COMMAND_OK) {
        log.logError("Error beginning transaction [%s]", PQerrorMessage(dbConnection));
        PQclear(queryResult);
        throw new Exception("Error opening transaction");
    }

    isTransactionActive = true;

    log.logDebug("Transaction - Open");

    PQclear(queryResult);
}

void Postgres::endTransaction()
{
	PGresult *			queryResult;
    
    if (isTransactionActive && isAutoTransaction) {
        isTransactionActive = false;
        isAutoTransaction = false;

        queryResult = PQexec(dbConnection, "END");

        if (PQresultStatus(queryResult) != PGRES_COMMAND_OK) {
            log.logError("Error ending transaction [%s]", PQerrorMessage(dbConnection));
            PQclear(queryResult);
            throw new Exception("Error ending transaction");
        }

        log.logDebug("Transaction - Close");

        PQclear(queryResult);
    }
}

PGresult * Postgres::_execute(const char * sql)
{
	PGresult *			queryResult;

    if (!isTransactionActive) {
        beginTransaction();
        isAutoTransaction = true;
    }

    queryResult = PQexec(dbConnection, sql);

    if (PQresultStatus(queryResult) != PGRES_COMMAND_OK && PQresultStatus(queryResult) != PGRES_TUPLES_OK) {
        log.logError("Error issuing statement [%s]: '%s'", sql, PQerrorMessage(dbConnection));

        if (queryResult != NULL) {
            PQclear(queryResult);
        }

        endTransaction();
        throw new Exception("Error issuing statement");
    }
    else {
        log.logInfo("Successfully executed statement [%s]", sql);
    }

    endTransaction();

    return queryResult;
}

void Postgres::getCalibrationData(char * szRowName, int16_t * offset, double * factor)
{
    PGresult *                      r;
    int                             rows = 0;
    int                             row;
    char                            szSelectStatement[256];

    sprintf(
        szSelectStatement, 
        "SELECT offset_amount, factor from %s where name = '%s'", 
        cfg.getValueAsCstr("calibration.dbtable"), 
        szRowName);
    
    r = _execute(szSelectStatement);

    rows = PQntuples(r);

    for (row = 0;row < rows;row++) {
        *offset = atoi(PQgetvalue(r, row, 0));
        *factor = atof(PQgetvalue(r, row, 1));

        log.logDebug("Got offset [%d] factor [%.3f] '%s'", *offset, *factor, szRowName);
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

    try {
        r = _execute(sql);
    }
    catch (Exception * e) {
        log.logError("Error issuing INSERT statement [%s]", sql);
        throw new Exception("Error issuing INSERT statement");
    }

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
