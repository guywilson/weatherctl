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
	char *			pszBody = NULL;
	char *			pszPath = NULL;
	const char *	type;

public:
	PostData() {
	}

	PostData(const char * type, char * path, char * body) : PostData() {
		setPath(path);
		setBody(body);

		this->type = type;
	}

	void		clean() {
		if (this->pszBody != NULL) {
			free(this->pszBody);
			this->pszBody = NULL;
		}
		if (this->pszPath != NULL) {
			free(this->pszPath);
			this->pszPath = NULL;
		}
	}

	~PostData() {
		printf("In PostData destructor...\n");
		clean();
	}

	const char *	getType() {
		return this->type;
	}
	void			setType(const char * type) {
		this->type = type;
	}
	char *		getBody() {
		return this->pszBody;
	}
	void		setBody(char * body) {
		this->pszBody = (char *)malloc(strlen(body));

		if (this->pszBody == NULL) {
			throw new Exception("Failed to allocate memeory for post data");
		}

		memcpy(this->pszBody, body, strlen(body));
		this->pszBody[strlen(body)] = 0;
	}
	char *		getPath() {
		return this->pszPath;
	}
	void		setPath(char * path) {
		this->pszPath = (char *)malloc(strlen(path));

		if (this->pszPath == NULL) {
			throw new Exception("Failed to allocate memeory for post data");
		}

		memcpy(this->pszPath, path, strlen(path));
		this->pszPath[strlen(path)] = 0;
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
