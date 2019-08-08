#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <pthread.h>

#include "currenttime.h"
#include "logger.h"

Logger::~Logger()
{
    closeLogger();
}

void Logger::initLogger(char * pszLogFileName, int logLevel)
{
    this->loggingLevel = logLevel;

    if (pszLogFileName != NULL && strlen(pszLogFileName) > 0) {
        this->lfp = fopen(pszLogFileName, "wt");

        if (this->lfp == NULL) {
	        syslog(LOG_INFO, "Failed to open log file %s", pszLogFileName);
            this->lfp = stdout;
        }
    }
    else {
        this->lfp = stdout;
    }
}

void Logger::closeLogger()
{
    if (lfp != stdout) {
        fclose(lfp);
    }
}

int Logger::getLogLevel()
{
    return this->loggingLevel;
}

void Logger::setLogLevel(int logLevel)
{
    this->loggingLevel = logLevel;
}

bool Logger::isLogLevel(int logLevel)
{
    return ((this->getLogLevel() & logLevel) == logLevel ? true : false);
}

int Logger::logMessage(int logLevel, bool addCR, const char * fmt, va_list args)
{
    int         bytesWritten = 0;

	pthread_mutex_lock(&mutex);

    if (this->loggingLevel & logLevel) {
        if (strlen(fmt) > MAX_LOG_LENGTH) {
            syslog(LOG_ERR, "Log line too long");
            return -1;
        }

        if (addCR) {
            strcpy(buffer, "[");
            strcat(buffer, currentTime.getTimeStamp());
            strcat(buffer, "] ");
            strcat(buffer, fmt);
            strcat(buffer, "\n");
        }
        else {
            strcpy(buffer, fmt);
        }

        bytesWritten = vfprintf(this->lfp, buffer, args);
        fflush(this->lfp);

        buffer[0] = 0;
    }

	pthread_mutex_unlock(&mutex);

    return bytesWritten;
}

void Logger::newline()
{
    fprintf(this->lfp, "\n");
}

int Logger::logInfo(const char * fmt, ...)
{
    va_list     args;
    int         bytesWritten;

    va_start (args, fmt);
    
    bytesWritten = logMessage(LOG_LEVEL_INFO, true, fmt, args);
    
    va_end(args);
    
    return bytesWritten;
}

int Logger::logDebug(const char * fmt, ...)
{
    va_list     args;
    int         bytesWritten;

    va_start (args, fmt);
    
    bytesWritten = logMessage(LOG_LEVEL_DEBUG, true, fmt, args);
    
    va_end(args);
    
    return bytesWritten;
}

int Logger::logDebugNoCR(const char * fmt, ...)
{
    va_list     args;
    int         bytesWritten;

    va_start (args, fmt);
    
    bytesWritten = logMessage(LOG_LEVEL_DEBUG, false, fmt, args);
    
    va_end(args);
    
    return bytesWritten;
}

int Logger::logError(const char * fmt, ...)
{
    va_list     args;
    int         bytesWritten;

    va_start (args, fmt);
    
    bytesWritten = logMessage(LOG_LEVEL_ERROR, true, fmt, args);
    
    va_end(args);
    
    return bytesWritten;
}

int Logger::logFatal(const char * fmt, ...)
{
    va_list     args;
    int         bytesWritten;

    va_start (args, fmt);
    
    bytesWritten = logMessage(LOG_LEVEL_FATAL, true, fmt, args);
    
    va_end(args);
    
    return bytesWritten;
}
