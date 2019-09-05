#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>

#include "exception.h"
#include "currenttime.h"
#include "webconnect.h"
#include "avrweather.h"
#include "queuemgr.h"
#include "logger.h"
#include "backup.h"
#include "configmgr.h"

extern "C" {
#include "mongoose.h"
}

#define INT_LENGTH				10

static void nullHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *	message;
	const char * 			szMsg = "HTTP/1.1 200 OK\r\n\r\n[No handler registered for URI '%s']";
	char *					pszMethod;
	char *					pszURI;

	Logger & log = Logger::getInstance();

	switch (event) {
		case MG_EV_HTTP_REQUEST:
			message = (struct http_message *)p;

			pszMethod = (char *)malloc(message->method.len + 1);

			if (pszMethod == NULL) {
				throw new Exception("Failed to allocate memory for method...");
			}

			memcpy(pszMethod, message->method.p, message->method.len);
			pszMethod[message->method.len] = 0;

			pszURI = (char *)malloc(message->uri.len + 1);

			if (pszURI == NULL) {
				throw new Exception("Failed to allocate memory for URI...");
			}

			memcpy(pszURI, message->uri.p, message->uri.len);
			pszURI[message->uri.len] = 0;

			log.logInfo("Null Handler: Got %s request for '%s'", pszMethod, pszURI);
			mg_printf(connection, szMsg, pszURI);
			connection->flags |= MG_F_SEND_AND_CLOSE;

			free(pszMethod);
			free(pszURI);
			break;

		default:
			break;
	}
}

WebConnector::WebConnector()
{
	this->pCurl = curl_easy_init();

	if (this->pCurl == NULL) {
		curl_easy_cleanup(pCurl);
		throw new Exception("Error initialising curl");
	}

	curl_easy_setopt(pCurl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
	curl_easy_setopt(pCurl, CURLOPT_ERRORBUFFER, this->szCurlError);
}

WebConnector::~WebConnector()
{
	curl_easy_cleanup(this->pCurl);
}

void WebConnector::queryConfig()
{
	static const char * keys[] = {
		"web.host", 
		"web.port", 
		"web.basepath", 
		"web.issecure", 
		"admin.listenport", 
		"admin.docroot"};

	const char *		pszToken;

	Logger & log = Logger::getInstance();

	ConfigManager & cfg = ConfigManager::getInstance();

	pszToken = cfg.getValueAsCstr(keys[0]);
	log.logDebug("Got '%s' as '%s'", keys[0], pszToken);
	strcpy(this->szHost, pszToken);

	pszToken = cfg.getValueAsCstr(keys[1]);
	log.logDebug("Got '%s' as '%s'", keys[1], pszToken);
	this->port = atoi(pszToken);

	pszToken = cfg.getValueAsCstr(keys[2]);
	log.logDebug("Got '%s' as '%s'", keys[2], pszToken);
	strcpy(this->szBasePath, pszToken);

	pszToken = cfg.getValueAsCstr(keys[3]);
	log.logDebug("Got '%s' as '%s'", keys[3], pszToken);
	this->isSecure = (strcmp(pszToken, "yes") == 0 || strcmp(pszToken, "true") == 0) ? true : false;

	pszToken = cfg.getValueAsCstr(keys[4]);
	log.logDebug("Got '%s' as '%s'", keys[4], pszToken);
	strcpy(this->szListenPort, pszToken);

	pszToken = cfg.getValueAsCstr(keys[5]);
	log.logDebug("Got '%s' as '%s'", keys[5], pszToken);
	strcpy(this->szDocRoot, pszToken);

	this->isConfigured = true;
}

int WebConnector::postTPH(PostData * pPostData)
{
	char				szBody[128];
	char				szWebPath[256];
	char				szPathSuffix[32];
	CURLcode			result;

	CurrentTime time;
	Logger & log = Logger::getInstance();

	sprintf(
		szBody,
		"{\n\t\"time\": \"%s\",\n\t\"save\": \"%s\",\n\t\"temperature\": \"%s\",\n\t\"pressure\": \"%s\",\n\t\"humidity\": \"%s\"\n}",
		pPostData->getTimestamp(),
		pPostData->isDoSave() ? "true" : "false",
		pPostData->getTemperature(),
		pPostData->getPressure(),
		pPostData->getHumidity());

	if (strcmp(pPostData->getType(), "AVG") == 0) {
		strcpy(szPathSuffix, WEB_PATH_AVG);
	}
	else if (strcmp(pPostData->getType(), "MAX") == 0) {
		strcpy(szPathSuffix, WEB_PATH_MAX);
	}
	else if (strcmp(pPostData->getType(), "MIN") == 0) {
		strcpy(szPathSuffix, WEB_PATH_MIN);
	}

	sprintf(
		szWebPath, 
		"%s://%s:%d%s%s", 
		(this->isSecure ? "https" : "http"), 
		this->getHost(), 
		this->getPort(), 
		this->szBasePath, 
		szPathSuffix);

	log.logDebug("Posting to %s [%s]", szWebPath, szBody);

	/*
	** Custom headers for JSON...
	*/
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "charsets: utf-8");

	curl_easy_setopt(pCurl, CURLOPT_URL, szWebPath);
	curl_easy_setopt(pCurl, CURLOPT_POSTFIELDSIZE, strlen(szBody));
	curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, szBody);
    curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers); 
    curl_easy_setopt(pCurl, CURLOPT_USERAGENT, "libcrp/0.1");

	result = curl_easy_perform(pCurl);

	if (result != CURLE_OK) {
		log.logError("Failed to post to %s - Curl error [%s]", szWebPath, this->szCurlError);
		return -1;
	}

	log.logDebug("Finished post to %s", szWebPath);

	return 0;
}

void WebConnector::initListener()
{
	Logger & log = Logger::getInstance();
	
	queryConfig();

	mg_mgr_init(&mgr, NULL);

	log.logInfo("Setting up listener on port %s", szListenPort);

	connection = mg_bind(&mgr, szListenPort, nullHandler);

	if (connection == NULL) {
		log.logError("Failed to bind to port %s", szListenPort);
		throw new Exception("Faled to bind to address");
	}

	log.logInfo("Bound default handler...");

	mg_set_protocol_http_websocket(connection);
}

void WebConnector::listen()
{
	Logger & log = Logger::getInstance();

	log.logInfo("Listening...");

	while (1) {
		mg_mgr_poll(&mgr, 1000);
	}

	mg_mgr_free(&mgr);
}

void WebConnector::registerHandler(const char * pszURI, void (* handler)(struct mg_connection *, int, void *))
{
	mg_register_http_endpoint(connection, pszURI, handler);
}
