#include <stdint.h>
#include <time.h>

#ifndef _INCL_CURRENTTIME
#define _INCL_CURRENTTIME

#define SECONDS_PER_MINUTE				60
#define SECONDS_PER_HOUR				(SECONDS_PER_MINUTE * 60)
#define SECONDS_PER_DAY					(SECONDS_PER_HOUR * 24)

class CurrentTime
{
private:
	struct tm *		localTime;
	char			szTimeStr[20];

public:
	CurrentTime();

	static void		initialiseUptimeClock();
	static char *	getUptime();
	static char *	getUptime(uint32_t uptimeSeconds);

	void			updateTime();
	void			updateTime(time_t * t);

	char *			getTimeStamp();

	int				getYear();
	int				getMonth();
	int				getDay();

	int				getHour();
	int				getMinute();
	int				getSecond();
};

#endif
