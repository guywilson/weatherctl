#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mongoose.h"
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
	char			szConfigFile[256];
	char			szHost[256];
	int				port;
	char			szListenPort[8];
	char			szBasePath[128];
	char			szDocRoot[256];

	bool			isConfigured = false;

	struct mg_mgr			mgr;
	struct mg_connection *	connection;

	WebConnector();

	void		queryConfig();

public:
	void		setConfigLocation(char * pszConfigFile);
	void		initListener();

	void		postTPH(const char * pszPathSuffix, bool save, char * pszTemperature, char * pszPressure, char * pszHumidity);
	void		registerHandler(const char * pszURI, void (* handler)(struct mg_connection *, int, void *));
	void		listen();

	char *		getHost() {
		return this->szHost;
	}
	int			getPort() {
		return this->port;
	}
	char *		getDocRoot() {
		return this->szDocRoot;
	}
};

void * webPostThread(void * pArgs);

#endif
