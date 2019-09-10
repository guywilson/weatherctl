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

#define CLASS_ID_BASE		0
#define CLASS_ID_TPH		1
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

	char *			getTimestamp() {
		return this->time.getTimeStamp();
	}
	virtual int				getClassID() {
		return CLASS_ID_BASE;
	}
	virtual char *		getJSON() = 0;
};

class PostDataVersion : public PostData
{
private:
	const char *	jsonTemplate = "{\n\t\"version\": \"%s\",\n\t\"buildDate\": \"%s\"\n}";

public:
	int				getClassID() {
		return CLASS_ID_VERSION;
	}
	char *			getJSON()
	{
		char *		jsonBuffer;

		jsonBuffer = (char *)malloc(strlen(jsonTemplate) + 80);

		sprintf(
			jsonBuffer,
			jsonTemplate,
			getWCTLVersion(),
			getWCTLBuildDate());

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
	PostDataTPH() {
	}

	PostDataTPH(const char * type, bool doSave, char * pszTemperature, char * pszPressure, char * pszHumidity) : PostDataTPH() {
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

	int				getClassID() {
		return CLASS_ID_TPH;
	}
	char *			getJSON()
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

	int			postTPH(PostDataTPH * pPostData);
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
