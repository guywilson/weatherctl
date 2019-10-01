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
    char        _name[64];
    char        _value[64];

    bool        _isNull;

public:
    PGField();
    PGField(const char * name, const char * value);
    ~PGField();

    bool            isNull();

    char *          getName();
    char *          getValue();
};

class PGRow
{
private:
    vector<PGField>    _fields;
    PGField             emptyField;

public:
    PGRow();

    int             getNumFields();

    void            addField(PGField & field);
    PGField &       getField(int fieldNum);
    PGField &       getField(string name);
};

class PGResult
{
private:
    vector<PGRow>   _rows;
    int             _numRows = 0;

public:
    PGResult(PGresult * results);
    ~PGResult();

    int             getNumRows();
    PGRow &         getRow(int index);
    PGRow &         getFirst();
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
    PGRow &                         find(const char * sql);
    PGResult &                      SELECT(const char * sql);
    int                             INSERT(const char * sql);
    int                             UPDATE(const char * sql);
    int                             DELETE(const char * sql);
};

#endif