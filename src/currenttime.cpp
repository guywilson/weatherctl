#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
	time_t				t;

	t = time(0);
	this->localTime = localtime(&t);
}

char * CurrentTime::getTimeStamp()
{
	updateTime();

	sprintf(
		this->szTimeStr,
		"%d-%02d-%02d %02d:%02d:%02d",
		getYear(),
		getMonth(),
		getDay(),
		getHour(),
		getMinute(),
		getSecond());

	return this->szTimeStr;
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

