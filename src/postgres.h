#include <map>
#include <vector>

#ifdef __APPLE__
#include <libpq-fe.h>
#else
#include <postgresql/libpq-fe.h>
#endif

#include "avrweather.h"
#include "configmgr.h"
#include "logger.h"

using namespace std;

#ifndef _INCL_POSTGRES
#define _INCL_POSTGRES

class PGField
{
private:
    string      name;
    string      value;

public:
    PGField(char * name, char * value);
    ~PGField();

    bool            isNull();

    string          getName();

    string          getValue();
    const char *    getValueAsCStr();
};

class PGRow
{
private:
    map<string, PGField>    _row;

public:
    PGRow();

    void            addField(PGField & field);
};

class PGResult
{
private:
//    PGresult *      _results;
    vector<PGRow>   rows;

public:
    PGResult(PGresult * results);
    ~PGResult();

    int             getNumRows();
    PGRow           getRow(int index);
    PGRow           getFirst();
};

class Postgres
{
private:
    PGconn *		dbConnection = NULL;
    Logger &        log = Logger::getInstance();
    ConfigManager & cfg = ConfigManager::getInstance();

    bool                            isTransactionActive = false;
    bool                            isAutoTransaction = false;

    PGresult *                      _execute(const char * sql);

public:
    Postgres(const char * host, int port, const char * dbName, const char * user, const char * password);
    ~Postgres();

    void                            beginTransaction();
    void                            endTransaction();

    void                            getCalibrationData(char * szRowName, int16_t * offset, double * factor);
    PGResult *                      find(const char * sql);
    PGResult *                      SELECT(const char * sql);
    int                             INSERT(const char * sql);
    int                             UPDATE(const char * sql);
    int                             DELETE(const char * sql);
};

#endif