#include <curl/curl.h>

#include "configmgr.h"
#include "exception.h"
#include "postdata.h"

#ifndef _INCL_REST
#define _INCL_REST

class Rest
{
private:
	char			szHost[256];
	int				port;
	bool			isSecure = false;
	char			szBasePath[128];
	char			szCurlError[CURL_ERROR_SIZE];
	CURL *			pCurl;

public:
    Rest();
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