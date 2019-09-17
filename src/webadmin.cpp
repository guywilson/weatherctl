#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>

#include "exception.h"
#include "webadmin.h"
#include "logger.h"
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

WebAdmin::WebAdmin()
{
	static const char * keys[] = {
		"admin.listenport", 
		"admin.docroot"};

	const char *		pszToken;

	Logger & log = Logger::getInstance();

	ConfigManager & cfg = ConfigManager::getInstance();

	pszToken = cfg.getValueAsCstr(keys[0]);
	log.logDebug("Got '%s' as '%s'", keys[0], pszToken);
	strcpy(this->szListenPort, pszToken);

	pszToken = cfg.getValueAsCstr(keys[1]);
	log.logDebug("Got '%s' as '%s'", keys[1], pszToken);
	strcpy(this->szHTMLDocRoot, pszToken);
	strcat(this->szHTMLDocRoot, "/html");
	strcpy(this->szCSSDocRoot, pszToken);

	initListener();
}

WebAdmin::~WebAdmin()
{
}

void WebAdmin::initListener()
{
	Logger & log = Logger::getInstance();
	
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

void WebAdmin::listen()
{
	Logger & log = Logger::getInstance();

	log.logInfo("Listening...");

	while (1) {
		mg_mgr_poll(&mgr, 1000);
	}

	mg_mgr_free(&mgr);
}

void WebAdmin::registerHandler(const char * pszURI, void (* handler)(struct mg_connection *, int, void *))
{
	mg_register_http_endpoint(connection, pszURI, handler);
}
