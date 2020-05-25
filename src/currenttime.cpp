#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "logger.h"
#include "currenttime.h"

time_t		startTime;

CurrentTime::CurrentTime()
{
	updateTime();
}

void CurrentTime::initialiseUptimeClock()
{
	startTime = time(0);
}

char * CurrentTime::getUptime()
{
	time_t		t;
	uint32_t	seconds;

	t = time(0);

	seconds = (uint32_t)difftime(t, startTime);

	return getUptime(seconds);
}

char * CurrentTime::getUptime(uint32_t uptimeSeconds)
{
	static char szUptime[128];
	uint32_t	t;
	int			days;
	int			hours;
	int			minutes;
	int			seconds;

	t = uptimeSeconds;

	days = t / SECONDS_PER_DAY;
	t = t % SECONDS_PER_DAY;

	hours = t / SECONDS_PER_HOUR;
	t = t % SECONDS_PER_HOUR;

	minutes = t / SECONDS_PER_MINUTE;
	t = t % SECONDS_PER_MINUTE;

	seconds = t;

	sprintf(
		szUptime, 
		"%d %s, %d %s, %d %s, %d %s", 
		days, 
		(days == 1 ? "day" : "days"),
		hours, 
		(hours == 1 ? "hour" : "hours"),
		minutes, 
		(minutes == 1 ? "minute" : "minutes"),
		seconds,
		(seconds == 1 ? "second" : "seconds"));

	return szUptime;
}

void CurrentTime::updateTime()
{
	struct timeval		tv;
	time_t				t;

	gettimeofday(&tv, NULL);

	t = tv.tv_sec;

	this->usec = tv.tv_usec;
	this->localTime = localtime(&t);
}

char * CurrentTime::getTimeStamp(bool includeMicroseconds)
{
	updateTime();

	if (includeMicroseconds) {
		sprintf(
			this->szTimeStr,
			"%d-%02d-%02d %02d:%02d:%02d.%06d",
			getYear(),
			getMonth(),
			getDay(),
			getHour(),
			getMinute(),
			getSecond(),
			getMicrosecond());
	}
	else {
		sprintf(
			this->szTimeStr,
			"%d-%02d-%02d %02d:%02d:%02d",
			getYear(),
			getMonth(),
			getDay(),
			getHour(),
			getMinute(),
			getSecond());
	}

	return this->szTimeStr;
}

char * CurrentTime::getTimeStamp()
{
	return this->getTimeStamp(false);
}

int CurrentTime::getYear()
{
	return localTime->tm_year + 1900;
}

int CurrentTime::getMonth()
{
	return localTime->tm_mon + 1;
}

int CurrentTime::getDay()
{
	return localTime->tm_mday;
}

int CurrentTime::getDayOfWeek()
{
	return localTime->tm_wday + 1;
}

int CurrentTime::getHour()
{
	return localTime->tm_hour;
}

int CurrentTime::getMinute()
{
	return localTime->tm_min;
}

int CurrentTime::getSecond()
{
	return localTime->tm_sec;
}

int CurrentTime::getMicrosecond()
{
	return this->usec;
}
