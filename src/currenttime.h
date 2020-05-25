#include <stdint.h>
#include <time.h>

#ifndef _INCL_CURRENTTIME
#define _INCL_CURRENTTIME

#define SECONDS_PER_MINUTE				60
#define SECONDS_PER_HOUR				(SECONDS_PER_MINUTE * 60)
#define SECONDS_PER_DAY					(SECONDS_PER_HOUR * 24)

#define SUNDAY							1
#define MONDAY							2
#define TUESDAY							3
#define WEDNESDAY						4
#define THURSDAY						5
#define FRIDAY							6
#define SATURDAY						7

class CurrentTime
{
private:
	struct tm *		localTime;
	int				usec;
	char			szTimeStr[28];

public:
	CurrentTime();

	static void		initialiseUptimeClock();
	static char *	getUptime();
	static char *	getUptime(uint32_t uptimeSeconds);

	void			updateTime();
	void			updateTime(time_t * t);

	char *			getTimeStamp(bool includeMicroseconds);
	char *			getTimeStamp();

	int				getYear();
	int				getMonth();
	int				getDay();
	int				getDayOfWeek();

	int				getHour();
	int				getMinute();
	int				getSecond();
	int				getMicrosecond();
};

#endif
