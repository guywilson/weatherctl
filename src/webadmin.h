#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <curl/curl.h>

#include "mongoose.h"
#include "currenttime.h"
#include "configmgr.h"
#include "exception.h"
#include "postdata.h"

extern "C" {
#include "version.h"
}

#ifndef _INCL_WEBCONNECT
#define _INCL_WEBCONNECT

class WebAdmin
{
public:
	static WebAdmin & getInstance()
	{
		static WebAdmin instance;
		return instance;
	}

private:
	char			szListenPort[8];
	char			szHTMLDocRoot[PATH_MAX];
	char			szCSSDocRoot[PATH_MAX];

	struct mg_mgr			mgr;
	struct mg_connection *	connection;

	WebAdmin();

	void		initListener();

public:
	~WebAdmin();

	void		registerHandler(const char * pszURI, void (* handler)(struct mg_connection *, int, void *));
	void		listen();

	char *		getHTMLDocRoot() {
		return this->szHTMLDocRoot;
	}
	char *		getCSSDocRoot() {
		return this->szCSSDocRoot;
	}
};

#endif
