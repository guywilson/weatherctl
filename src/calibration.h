#include <stdint.h>

#include "configmgr.h"
#include "logger.h"

#ifndef _INCL_CALIBRATION
#define _INCL_CALIBRATION

#define NUM_CALIBRATION_PAIRS                5
#define STR_VALUE_LENGTH                    32

typedef struct
{
    int16_t         thermometerOffset;
    double          thermometerFactor;

    int16_t         barometerOffset;
    double          barometerFactor;

    int16_t         humidityOffset;
    double          humidityFactor;

    int16_t         anemometerOffset;
    double          anemometerFactor;

    int16_t         rainGaugeOffset;
    double          rainGaugeFactor;
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
    const char *    _dbRowNames[NUM_CALIBRATION_PAIRS] = {"thermometer", "barometer", "humidity", "anemometer", "rain"};

    int16_t         _offsets[NUM_CALIBRATION_PAIRS];
    double          _factors[NUM_CALIBRATION_PAIRS];

    char            _strOffsets[NUM_CALIBRATION_PAIRS][STR_VALUE_LENGTH];
    char            _strFactors[NUM_CALIBRATION_PAIRS][STR_VALUE_LENGTH];

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

    int16_t         getOffset(SensorType t);
    char *          getOffsetAsCStr(SensorType t);
    void            setOffset(SensorType t, int16_t offset);
    void            setOffset(SensorType t, char * pszOffset);

    double          getFactor(SensorType t);
    char *          getFactorAsCStr(SensorType t);
    void            setFactor(SensorType t, double factor);
    void            setFactor(SensorType t, char * pszFactor);
};

#endif