#include <time.h>

#ifndef _INCL_CURRENTTIME
#define _INCL_CURRENTTIME

class CurrentTime
{
private:
	struct tm *		localTime;
	char			szTimeStr[20];

public:
	CurrentTime();

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
