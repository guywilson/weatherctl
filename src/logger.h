#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "currenttime.h"

#ifndef _INCL_LOGGER
#define _INCL_LOGGER

#define MAX_LOG_LENGTH          250

/*
** Supported log levels...
*/
#define LOG_LEVEL_INFO          0x01
#define LOG_LEVEL_STATUS        0x02
#define LOG_LEVEL_DEBUG         0x04
#define LOG_LEVEL_ERROR         0x08
#define LOG_LEVEL_FATAL         0x10

#define LOG_LEVEL_ALL           (LOG_LEVEL_INFO | LOG_LEVEL_STATUS | LOG_LEVEL_DEBUG | LOG_LEVEL_ERROR | LOG_LEVEL_FATAL)

class Logger
{
public:
    static Logger & getInstance() {
        static Logger instance;
        return instance;
    }

private:
    Logger() {}

    FILE *          lfp;
    int             loggingLevel;
    char            buffer[512];
    pthread_mutex_t mutex;

    CurrentTime     currentTime;

    int             logLevel_atoi(const char * pszLoggingLevel);
    int             logMessage(int logLevel, bool addCR, const char * fmt, va_list args);

public:
    ~Logger();

    void        initLogger(const char * pszLogFileName, int logLevel);
    void        initLogger(const char * pszLogFileName, const char * pszLoggingLevel);
    void        initLogger(int logLevel);
    
    void        closeLogger();

    int         getLogLevel();
    void        setLogLevel(int logLevel);
    void        setLogLevel(const char * pszLogLevel);
    bool        isLogLevel(int logLevel);

    void        newline();
    int         logInfo(const char * fmt, ...);
    int         logStatus(const char * fmt, ...);
    int         logDebug(const char * fmt, ...);
    int         logDebugNoCR(const char * fmt, ...);
    int         logError(const char * fmt, ...);
    int         logFatal(const char * fmt, ...);
};

#endif