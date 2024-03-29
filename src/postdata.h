#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "avrweather.h"
#include "currenttime.h"

#ifndef _INCL_POSTDATA
#define _INCL_POSTDATA

#define WEB_PATH_AVG		"avg-tph"
#define WEB_PATH_MIN		"min-tph"
#define WEB_PATH_MAX		"max-tph"

#define WEB_PATH_VERSION	"version"
#define WEB_PATH_CLEANUP	"cleanup"
#define WEB_PATH_LOGIN		"auth/login"

#define CLASS_ID_BASE		0
#define CLASS_ID_TPH		1
#define CLASS_ID_WINDSPEED	2
#define CLASS_ID_RAINFALL	3
#define CLASS_ID_LOGIN		4
#define CLASS_ID_CLEANUP	5
#define CLASS_ID_VERSION	9

class PostData
{
private:
	CurrentTime		time;

protected:

public:
	PostData() {
	}

	virtual ~PostData() {
	}

	char *	getTimestamp() {
		return this->time.getTimeStamp();
	}
	virtual int	getClassID() {
		return CLASS_ID_BASE;
	}

	virtual const char *	getPathSuffix() = 0;
	virtual char *			getJSON() = 0;
};

class PostDataLogin : public PostData
{
private:
	char			szEmailAddress[80];
	char			szPassword[128];
	const char *	jsonTemplate = "{\n\t\"email\": \"%s\",\n\t\"password\": \"%s\"\n}";

public:
	PostDataLogin(const char * pszEmail, const char * pszPassword) {
		strncpy(this->szEmailAddress, pszEmail, sizeof(this->szEmailAddress));
		strncpy(this->szPassword, pszPassword, sizeof(this->szPassword));
	}
	int	getClassID() {
		return CLASS_ID_LOGIN;
	}
	const char * getPathSuffix() {
		return WEB_PATH_LOGIN; 
	}
	char *	getJSON()
	{
		char *		jsonBuffer;

		jsonBuffer = (char *)malloc(strlen(jsonTemplate) + 80);

		sprintf(
			jsonBuffer,
			jsonTemplate,
			this->szEmailAddress,
			this->szPassword);

		return jsonBuffer;
	}
};

class PostDataCleanup : public PostData
{
private:
	const char *	jsonTemplate = "{\n}";

public:
	PostDataCleanup() {}

	int	getClassID() {
		return CLASS_ID_CLEANUP;
	}
	const char * getPathSuffix() {
		return WEB_PATH_CLEANUP; 
	}
	char *	getJSON() {
		char *		jsonBuffer;

		jsonBuffer = (char *)malloc(strlen(jsonTemplate) + 1);

		strcpy(jsonBuffer, jsonTemplate);
		
		return jsonBuffer;
	}
};

class PostDataVersion : public PostData
{
private:
	char			szWCTLVersion[20];
	char			szWCTLBuildDate[20];
	char			szAVRVersion[20];
	char			szAVRBuildDate[20];
	const char *	jsonTemplate = "{\n\t\"wctlVersion\": \"%s\",\n\t\"wctlBuildDate\": \"%s\",\n\t\"avrVersion\": \"%s\",\n\t\"avrBuildDate\": \"%s\"\n}";

public:
	PostDataVersion(const char * pszWCTLVersion, const char * pszWCTLBuildDate, char * pszAVRVersion, char * pszAVRBuildDate) {
		strncpy(this->szWCTLVersion, pszWCTLVersion, sizeof(this->szWCTLVersion));
		strncpy(this->szWCTLBuildDate, pszWCTLBuildDate, sizeof(this->szWCTLBuildDate));
		strncpy(this->szAVRVersion, pszAVRVersion, sizeof(this->szAVRVersion));
		strncpy(this->szAVRBuildDate, pszAVRBuildDate, sizeof(this->szAVRBuildDate));
	}
	int	getClassID() {
		return CLASS_ID_VERSION;
	}
	const char * getPathSuffix() {
		return WEB_PATH_VERSION; 
	}
	char *	getJSON()
	{
		char *		jsonBuffer;

		jsonBuffer = (char *)malloc(strlen(jsonTemplate) + 80);

		sprintf(
			jsonBuffer,
			jsonTemplate,
			this->szWCTLVersion,
			this->szWCTLBuildDate,
			this->szAVRVersion,
			this->szAVRBuildDate);

		return jsonBuffer;
	}
};

class PostDataTPH : public PostData
{
private:
	char 			szTemperature[20];
	char			szPressure[20];
	char			szHumidity[20];
	char			szDewPoint[20];
	bool			doSave = false;
	const char *	type;
	const char *	jsonTemplate = "{\n\t\"time\": \"%s\",\n\t\"save\": \"%s\",\n\t\"temperature\": \"%s\",\n\t\"pressure\": \"%s\",\n\t\"humidity\": \"%s\",\n\t\"dewpoint\": \"%s\"\n}";

public:
	PostDataTPH(const char * type, bool doSave, TPH * pTPH) {
		double t = getActualTemperature(pTPH->temperature);
		double p = getActualPressure(pTPH->pressure);
		double h = getActualHumidity(pTPH->humidity, t);
		double d = getActualDewPoint(t, h);

		sprintf(this->szTemperature, "%.2f", t);
		sprintf(this->szPressure, "%.2f", p);
		sprintf(this->szHumidity, "%.2f", h);
		sprintf(this->szDewPoint, "%.2f", d);

		this->doSave = doSave;
		this->type = type;
	}

	void		clean() {
		memset(this->szTemperature, 0, sizeof(this->szTemperature));
		memset(this->szPressure, 0, sizeof(this->szPressure));
		memset(this->szHumidity, 0, sizeof(this->szHumidity));
		memset(this->szDewPoint, 0, sizeof(this->szDewPoint));
	}

	~PostDataTPH() {
		clean();
	}

	int	getClassID() {
		return CLASS_ID_TPH;
	}
	const char * getPathSuffix() {
		const char * pszPathSuffix = "";

		if (strcmp(this->getType(), "AVG") == 0) {
			pszPathSuffix = WEB_PATH_AVG;
		}
		else if (strcmp(this->getType(), "MAX") == 0) {
			pszPathSuffix = WEB_PATH_MAX;
		}
		else if (strcmp(this->getType(), "MIN") == 0) {
			pszPathSuffix = WEB_PATH_MIN;
		}

		return pszPathSuffix;
	}
	char *	getJSON()
	{
		char *		jsonBuffer;

		jsonBuffer = (char *)malloc(strlen(jsonTemplate) + sizeof(szTemperature) + sizeof(szPressure) + sizeof(szHumidity) + sizeof(szDewPoint) + 16);

		sprintf(
			jsonBuffer,
			jsonTemplate,
			this->getTimestamp(),
			this->isDoSave() ? "true" : "false",
			this->getTemperature(),
			this->getPressure(),
			this->getHumidity(),
			this->getDewPoint());

		return jsonBuffer;
	}
	const char *	getType() {
		return this->type;
	}
	bool		isDoSave() {
		return this->doSave;
	}
	char *		getTemperature() {
		return this->szTemperature;
	}
	char *		getPressure() {
		return this->szPressure;
	}
	char *		getHumidity() {
		return this->szHumidity;
	}
	char *		getDewPoint() {
		return this->szDewPoint;
	}
};

class PostDataWindspeed : public PostData
{
private:
	char			szAvgWindspeed[20];
	char			szMaxWindspeed[20];
	bool			doSaveAvg = false;
	bool			doSaveMax = false;
	const char *	jsonTemplate = "{\n\t\"time\": \"%s\",\n\t\"saveAvg\": \"%s\",\n\t\"saveMax\": \"%s\",\n\t\"avgWindspeed\": \"%s\",\n\t\"maxWindspeed\": \"%s\"\n}";

	void clean() {
		memset(this->szAvgWindspeed, 0, sizeof(this->szAvgWindspeed));
		memset(this->szMaxWindspeed, 0, sizeof(this->szMaxWindspeed));

		this->doSaveAvg = false;
		this->doSaveMax = false;
	}

public:
	PostDataWindspeed(bool doSaveAvg, bool doSaveMax, WINDSPEED * pWindspeed) {
		this->doSaveAvg = doSaveAvg;
		this->doSaveMax = doSaveMax;

		sprintf(this->szAvgWindspeed, "%.2f", getActualWindspeed(pWindspeed->avgWindspeed));
		sprintf(this->szMaxWindspeed, "%.2f", getActualWindspeed(pWindspeed->maxWindspeed));
	}

	~PostDataWindspeed() {
		clean();
	}

	int	getClassID() {
		return CLASS_ID_WINDSPEED;
	}
	const char * getPathSuffix() {
		return "wind";
	}
	char *	getJSON()
	{
		char *		jsonBuffer;

		jsonBuffer = (char *)malloc(strlen(jsonTemplate) + sizeof(szAvgWindspeed) + sizeof(szMaxWindspeed) + 24);

		sprintf(
			jsonBuffer,
			jsonTemplate,
			this->getTimestamp(),
			this->doSaveAvg ? "true" : "false",
			this->doSaveMax ? "true" : "false",
			this->szAvgWindspeed,
			this->szMaxWindspeed);

		return jsonBuffer;
	}

	bool isDoSaveAvg() {
		return this->doSaveAvg;
	}
	bool isDoSaveMax() {
		return this->doSaveMax;
	}
	char * getAvgWindspeed() {
		return this->szAvgWindspeed;
	}
	char * getMaxWindspeed() {
		return this->szMaxWindspeed;
	}
};

class PostDataRainfall : public PostData
{
private:
	char			szAvgRainfall[20];
	char			szTotalRainfall[20];
	bool			doSaveAvg = false;
	bool			doSaveTotal = false;
	const char *	jsonTemplate = "{\n\t\"time\": \"%s\",\n\t\"saveAvg\": \"%s\",\n\t\"saveTotal\": \"%s\",\n\t\"avgRainfall\": \"%s\",\n\t\"totalRainfall\": \"%s\"\n}";

	void clean() {
		memset(this->szAvgRainfall, 0, sizeof(this->szAvgRainfall));
		memset(this->szTotalRainfall, 0, sizeof(this->szTotalRainfall));

		this->doSaveAvg = false;
		this->doSaveTotal = false;
	}

public:
	PostDataRainfall(bool doSaveAvg, bool doSaveTotal, RAINFALL * pRainfall) {
		this->doSaveAvg = doSaveAvg;
		this->doSaveTotal = doSaveTotal;

		sprintf(this->szAvgRainfall, "%.2f", getActualRainfall(pRainfall->avgRainfall));
		sprintf(this->szTotalRainfall, "%.2f", getActualRainfall(pRainfall->totalRainfall));
	}

	~PostDataRainfall() {
		clean();
	}

	int	getClassID() {
		return CLASS_ID_RAINFALL;
	}
	const char * getPathSuffix() {
		return "rain";
	}
	char *	getJSON()
	{
		char *		jsonBuffer;

		jsonBuffer = (char *)malloc(strlen(jsonTemplate) + sizeof(szAvgRainfall) + sizeof(szTotalRainfall) + 24);

		sprintf(
			jsonBuffer,
			jsonTemplate,
			this->getTimestamp(),
			this->doSaveAvg ? "true" : "false",
			this->doSaveTotal ? "true" : "false",
			this->szAvgRainfall,
			this->szTotalRainfall);

		return jsonBuffer;
	}

	bool isDoSaveAvg() {
		return this->doSaveAvg;
	}
	bool isDoSaveTotal() {
		return this->doSaveTotal;
	}
	char * getAvgRainfall() {
		return this->szAvgRainfall;
	}
	char * getTotalRainfall() {
		return this->szTotalRainfall;
	}
};

#endif