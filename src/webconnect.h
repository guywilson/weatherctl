#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <curl/curl.h>

#include "mongoose.h"
#include "currenttime.h"
#include "configmgr.h"
#include "exception.h"

extern "C" {
#include "version.h"
}

#ifndef _INCL_WEBCONNECT
#define _INCL_WEBCONNECT

#define WEB_PATH_AVG		"avg-tph"
#define WEB_PATH_MIN		"min-tph"
#define WEB_PATH_MAX		"max-tph"

#define WEB_PATH_VERSION	"version"

#define CLASS_ID_BASE		0
#define CLASS_ID_TPH		1
#define CLASS_ID_WINDSPEED	2
#define CLASS_ID_RAINFALL	3
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

class PostDataVersion : public PostData
{
private:
	const char *	jsonTemplate = "{\n\t\"version\": \"%s\",\n\t\"buildDate\": \"%s\"\n}";

public:
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
			getVersion(),
			getBuildDate());

		return jsonBuffer;
	}
};

class PostDataTPH : public PostData
{
private:
	char 			szTemperature[20];
	char			szPressure[20];
	char			szHumidity[20];
	bool			doSave = false;
	const char *	type;
	const char *	jsonTemplate = "{\n\t\"time\": \"%s\",\n\t\"save\": \"%s\",\n\t\"temperature\": \"%s\",\n\t\"pressure\": \"%s\",\n\t\"humidity\": \"%s\"\n}";

public:
	PostDataTPH(const char * type, bool doSave, char * pszTemperature, char * pszPressure, char * pszHumidity) {
		strncpy(this->szTemperature, pszTemperature, sizeof(this->szTemperature));
		strncpy(this->szPressure, pszPressure, sizeof(this->szPressure));
		strncpy(this->szHumidity, pszHumidity, sizeof(this->szHumidity));

		this->doSave = doSave;
		this->type = type;
	}

	void		clean() {
		memset(this->szTemperature, 0, sizeof(this->szTemperature));
		memset(this->szPressure, 0, sizeof(this->szPressure));
		memset(this->szHumidity, 0, sizeof(this->szHumidity));
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

		jsonBuffer = (char *)malloc(strlen(jsonTemplate) + sizeof(szTemperature) + sizeof(szPressure) + sizeof(szHumidity) + 12);

		sprintf(
			jsonBuffer,
			jsonTemplate,
			this->getTimestamp(),
			this->isDoSave() ? "true" : "false",
			this->getTemperature(),
			this->getPressure(),
			this->getHumidity());

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
	PostDataWindspeed(bool doSaveAvg, bool doSaveMax, char * pszAvgWindspeed, char * pszMaxWindspeed) {
		this->doSaveAvg = doSaveAvg;
		this->doSaveMax = doSaveMax;

		strncpy(this->szAvgWindspeed, pszAvgWindspeed, sizeof(this->szAvgWindspeed));
		strncpy(this->szMaxWindspeed, pszMaxWindspeed, sizeof(this->szMaxWindspeed));
	}

	~PostDataWindspeed() {
		clean();
	}

	int	getClassID() {
		return CLASS_ID_WINDSPEED;
	}
	const char * getPathSuffix() {
		return "windspeed";
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
	PostDataRainfall(bool doSaveAvg, bool doSaveTotal, char * pszAvgRainfall, char * pszTotalRainfall) {
		this->doSaveAvg = doSaveAvg;
		this->doSaveTotal = doSaveTotal;

		strncpy(this->szAvgRainfall, pszAvgRainfall, sizeof(this->szAvgRainfall));
		strncpy(this->szTotalRainfall, pszTotalRainfall, sizeof(this->szTotalRainfall));
	}

	~PostDataRainfall() {
		clean();
	}

	int	getClassID() {
		return CLASS_ID_RAINFALL;
	}
	const char * getPathSuffix() {
		return "rainfall";
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

class WebConnector
{
public:
	static WebConnector & getInstance()
	{
		static WebConnector instance;
		return instance;
	}

private:
	char			szHost[256];
	int				port;
	bool			isSecure = false;
	char			szListenPort[8];
	char			szBasePath[128];
	char			szHTMLDocRoot[PATH_MAX];
	char			szCSSDocRoot[PATH_MAX];
	char			szCurlError[CURL_ERROR_SIZE];

	bool			isConfigured = false;

	CURL *			pCurl;

	struct mg_mgr			mgr;
	struct mg_connection *	connection;

	WebConnector();

	void		queryConfig();

public:
	~WebConnector();

	void		initListener();

	int			post(PostData * pPostData);
	void		registerHandler(const char * pszURI, void (* handler)(struct mg_connection *, int, void *));
	void		listen();

	char *		getHost() {
		return this->szHost;
	}
	int			getPort() {
		return this->port;
	}

	char *		getHTMLDocRoot() {
		return this->szHTMLDocRoot;
	}
	char *		getCSSDocRoot() {
		return this->szCSSDocRoot;
	}
};

#endif
