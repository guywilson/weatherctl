#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <curl/curl.h>

#include "mongoose.h"
#include "currenttime.h"
#include "exception.h"

#ifndef _INCL_WEBCONNECT
#define _INCL_WEBCONNECT

#define WEB_PATH_AVG		"avg-tph"
#define WEB_PATH_MIN		"min-tph"
#define WEB_PATH_MAX		"max-tph"

class PostData
{
private:
	char 			szTemperature[20];
	char			szPressure[20];
	char			szHumidity[20];
	bool			doSave = false;
	const char *	type;
	CurrentTime		time;

public:
	PostData() {
	}

	PostData(const char * type, bool doSave, char * pszTemperature, char * pszPressure, char * pszHumidity) : PostData() {
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

	~PostData() {
		clean();
	}

	char *			getTimestamp() {
		return this->time.getTimeStamp();
	}
	const char *	getType() {
		return this->type;
	}
	void			setType(const char * type) {
		this->type = type;
	}
	bool		isDoSave() {
		return this->doSave;
	}
	void		setDoSave(bool doSave) {
		this->doSave = doSave;
	}
	char *		getTemperature() {
		return this->szTemperature;
	}
	void		setTemperature(char * pszTemperature) {
		strncpy(this->szTemperature, pszTemperature, sizeof(this->szTemperature));
	}
	char *		getPressure() {
		return this->szPressure;
	}
	void		setPressure(char * pszPressure) {
		strncpy(this->szPressure, pszPressure, sizeof(this->szPressure));
	}
	char *		getHumidity() {
		return this->szHumidity;
	}
	void		setHumidity(char * pszHumidity) {
		strncpy(this->szHumidity, pszHumidity, sizeof(this->szHumidity));
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

	int			postTPH(PostData * pPostData);
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
