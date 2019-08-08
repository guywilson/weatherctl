#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "currenttime.h"

CurrentTime::CurrentTime()
{
	updateTime();
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

