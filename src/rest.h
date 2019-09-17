#include <curl/curl.h>

#include "configmgr.h"
#include "exception.h"
#include "postdata.h"

#ifndef _INCL_REST
#define _INCL_REST

class Rest
{
public:
	static Rest & getInstance()
	{
		static Rest instance;
		return instance;
	}

private:
	char			szHost[256];
	int				port;
	bool			isSecure = false;
	char			szBasePath[128];
	char			szCurlError[CURL_ERROR_SIZE];
	CURL *			pCurl;

    Rest();

public:
    ~Rest();

	char *		getHost() {
		return this->szHost;
	}
	int			getPort() {
		return this->port;
	}

	int			post(PostData * pPostData);
};

#endif