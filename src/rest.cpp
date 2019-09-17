#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>

#include "exception.h"
#include "logger.h"
#include "configmgr.h"
#include "postdata.h"
#include "queuemgr.h"
#include "frame.h"
#include "avrweather.h"
#include "rest.h"

extern "C" {
#include "version.h"
}

Rest::Rest()
{
	this->pCurl = curl_easy_init();

	if (this->pCurl == NULL) {
		curl_easy_cleanup(pCurl);
		throw new Exception("Error initialising curl");
	}

	curl_easy_setopt(pCurl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
	curl_easy_setopt(pCurl, CURLOPT_ERRORBUFFER, this->szCurlError);

	static const char * keys[] = {
		"web.host", 
		"web.port", 
		"web.basepath", 
		"web.issecure"}; 

	const char *		pszToken;

	Logger & log = Logger::getInstance();

	ConfigManager & cfg = ConfigManager::getInstance();

	pszToken = cfg.getValueAsCstr(keys[0]);
	log.logDebug("Got '%s' as '%s'", keys[0], pszToken);
	strcpy(this->szHost, pszToken);

	pszToken = cfg.getValueAsCstr(keys[1]);
	log.logDebug("Got '%s' as '%s'", keys[1], pszToken);
	this->port = cfg.getValueAsInteger(keys[1]);

	pszToken = cfg.getValueAsCstr(keys[2]);
	log.logDebug("Got '%s' as '%s'", keys[2], pszToken);
	strcpy(this->szBasePath, pszToken);

	pszToken = cfg.getValueAsCstr(keys[3]);
	log.logDebug("Got '%s' as '%s'", keys[3], pszToken);
	this->isSecure = cfg.getValueAsBoolean(keys[3]);
}

Rest::~Rest()
{
	curl_easy_cleanup(this->pCurl);
}

int	Rest::post(PostData * pPostData)
{
	char *				pszBody;
	char				szWebPath[256];
	CURLcode			result;

	Logger & log = Logger::getInstance();

	pszBody = pPostData->getJSON();

	sprintf(
		szWebPath, 
		"%s://%s:%d%s%s", 
		(this->isSecure ? "https" : "http"), 
		this->getHost(), 
		this->getPort(), 
		this->szBasePath, 
		pPostData->getPathSuffix());

	log.logDebug("Posting to %s [%s]", szWebPath, pszBody);

	/*
	** Custom headers for JSON...
	*/
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "charsets: utf-8");

	curl_easy_setopt(pCurl, CURLOPT_URL, szWebPath);
	curl_easy_setopt(pCurl, CURLOPT_POSTFIELDSIZE, strlen(pszBody));
	curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, pszBody);
    curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers); 
    curl_easy_setopt(pCurl, CURLOPT_USERAGENT, "libcrp/0.1");

	result = curl_easy_perform(pCurl);

	free(pszBody);

	if (result != CURLE_OK) {
		log.logError("Failed to post to %s - Curl error [%s]", szWebPath, this->szCurlError);
		return -1;
	}

	log.logDebug("Finished post to %s", szWebPath);

	return 0;
}
