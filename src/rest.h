#include <curl/curl.h>

#include "configmgr.h"
#include "exception.h"
#include "postdata.h"

#ifndef _INCL_REST
#define _INCL_REST

#define POST_OK						0
#define POST_CURL_ERROR				-1
#define POST_AUTHENTICATION_ERROR	-2

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

	char *			login(char * pszLoginDetails, const char * pszPathSuffix);
	int				post(PostData * pPostData, const char * pszAPIKey);
};

#endif