#include <string>
#include <iostream>
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
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

extern "C" {
#include "version.h"
}

using namespace std;
using namespace rapidjson;

size_t CurlWrite_CallbackFunc(void * contents, size_t size, size_t nmemb, std::string * s)
{
    size_t newLength = size * nmemb;

	Logger & log = Logger::getInstance();

    try {
        s->append((char*)contents, newLength);
    }
    catch(std::bad_alloc & e) {
        log.logError("Failed to grow string");
        return 0;
    }

    return newLength;
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

	pszToken = cfg.getValue(keys[0]);
	log.logDebug("Got '%s' as '%s'", keys[0], pszToken);
	strcpy(this->szHost, pszToken);

	pszToken = cfg.getValue(keys[1]);
	log.logDebug("Got '%s' as '%s'", keys[1], pszToken);
	this->port = cfg.getValueAsInteger(keys[1]);

	pszToken = cfg.getValue(keys[2]);
	log.logDebug("Got '%s' as '%s'", keys[2], pszToken);
	strcpy(this->szBasePath, pszToken);

	pszToken = cfg.getValue(keys[3]);
	log.logDebug("Got '%s' as '%s'", keys[3], pszToken);
	this->isSecure = cfg.getValueAsBoolean(keys[3]);
}

Rest::~Rest()
{
	curl_easy_cleanup(this->pCurl);
}

const char * Rest::login(char * pszLoginDetails, const char * pszPathSuffix)
{
	char				szWebPath[512];
	CURLcode			result;

	Logger & log = Logger::getInstance();

	sprintf(
		szWebPath, 
		"%s://%s:%d%s%s", 
		(this->isSecure ? "https" : "http"), 
		this->getHost(), 
		this->getPort(), 
		this->szBasePath, 
		pszPathSuffix);

	log.logDebug("logging in to %s", szWebPath);

	string response;

	/*
	** Custom headers for JSON...
	*/
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "charsets: utf-8");

	curl_easy_setopt(pCurl, CURLOPT_URL, szWebPath);
	curl_easy_setopt(pCurl, CURLOPT_POSTFIELDSIZE, strlen(pszLoginDetails));
	curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, pszLoginDetails);
    curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers); 
    curl_easy_setopt(pCurl, CURLOPT_USERAGENT, "libcrp/0.1");
	curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, CurlWrite_CallbackFunc);
	curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &response);

	result = curl_easy_perform(pCurl);

	if (result != CURLE_OK) {
		log.logError("Failed to post to %s - Curl error [%s]", szWebPath, this->szCurlError);
		return NULL;
	}

	Document d;

	d.Parse(response.c_str());

	Value & s = d["token"];

	return s.GetString();
}

int	Rest::post(PostData * pPostData, const char * pszAPIKey)
{
	char *				pszBody;
	char *				pszTokenHeader;
	char				szWebPath[512];
	CURLcode			result;

	Logger & log = Logger::getInstance();

	pszTokenHeader = (char *)malloc(strlen(pszAPIKey) + 32);

	if (pszTokenHeader == NULL) {
		log.logError("Failed to allocate token header");
		throw new Exception("Failed to allocate token header");
	}

	pszBody = pPostData->getJSON();

	strcpy(pszTokenHeader, "x-access-token: ");
	strcat(pszTokenHeader, pszAPIKey);

	sprintf(
		szWebPath, 
		"%s://%s:%d%s%s", 
		(this->isSecure ? "https" : "http"), 
		this->getHost(), 
		this->getPort(), 
		this->szBasePath, 
		pPostData->getPathSuffix());

	log.logDebug("Posting to %s [%s]", szWebPath, pszBody);

	string response;

	/*
	** Custom headers for JSON...
	*/
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "charsets: utf-8");
	headers = curl_slist_append(headers, pszTokenHeader);

	curl_easy_setopt(pCurl, CURLOPT_URL, szWebPath);
	curl_easy_setopt(pCurl, CURLOPT_POSTFIELDSIZE, strlen(pszBody));
	curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, pszBody);
    curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers); 
    curl_easy_setopt(pCurl, CURLOPT_USERAGENT, "libcrp/0.1");
	curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, CurlWrite_CallbackFunc);
	curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &response);

	result = curl_easy_perform(pCurl);

	free(pszBody);
	free(pszTokenHeader);

	if (result != CURLE_OK) {
		log.logError("Failed to post to %s - Curl error [%s]", szWebPath, this->szCurlError);
		return POST_CURL_ERROR;
	}

	log.logDebug("Finished post to %s", szWebPath);
	log.logInfo("Got response from server: %s", response.c_str());

	Document d;

	d.Parse(response.c_str());

	Value & s = d["auth"];

	bool isAuthenticated = s.GetBool();

	if (!isAuthenticated) {
		return POST_AUTHENTICATION_ERROR;
	}

	return POST_OK;
}
