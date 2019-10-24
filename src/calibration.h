#include <stdint.h>

#include "configmgr.h"
#include "logger.h"

#ifndef _INCL_CALIBRATION
#define _INCL_CALIBRATION

#define NUM_CALIBRATIONS                     5
#define STR_VALUE_LENGTH                    32

typedef struct
{
    double          offset;
    double          factor;
}
CALIBRATION;

typedef struct
{
    CALIBRATION     thermometer;
    CALIBRATION     barometer;
    CALIBRATION     hygrometer;
    CALIBRATION     anemometer;
    CALIBRATION     rainGauge;
}
CALIBRATION_DATA;

typedef CALIBRATION_DATA *     PCALIBRATION_DATA;

class CalibrationData
{
public:
    static CalibrationData & getInstance() {
        static CalibrationData instance;
        return instance;
    }

private:
    CalibrationData();

    Logger & log = Logger::getInstance();
    ConfigManager & cfg = ConfigManager::getInstance();

    char            _dbTableName[64];
    const char *    _dbRowNames[NUM_CALIBRATIONS] = {"thermometer", "barometer", "hygrometer", "anemometer", "rainGauge"};

    CALIBRATION     _calibrations[NUM_CALIBRATIONS];

    char            _strOffsets[NUM_CALIBRATIONS][STR_VALUE_LENGTH];
    char            _strFactors[NUM_CALIBRATIONS][STR_VALUE_LENGTH];

    bool            isStale = true;

public:
    ~CalibrationData() {}

    enum SensorType {
        thermometer = 0,
        barometer   = 1,
        hygrometer  = 2,
        anemometer  = 3,
        rainGauge   = 4
    };

    void            retrieve();
    void            save();

    CALIBRATION     getCalibration(SensorType t);
    void            setCalibration(SensorType t, CALIBRATION c);

    double          getOffset(SensorType t);
    char *          getOffsetAsCStr(SensorType t);
    void            setOffset(SensorType t, double offset);
    void            setOffset(SensorType t, char * pszOffset);
    void            setOffset(SensorType t, const char * pszOffset);

    double          getFactor(SensorType t);
    char *          getFactorAsCStr(SensorType t);
    void            setFactor(SensorType t, double factor);
    void            setFactor(SensorType t, char * pszFactor);
    void            setFactor(SensorType t, const char * pszFactor);
};

#endif