#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>

#include "wctl_error.h"
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
				throw wctl_error(wctl_error::buildMsg("Failed to allocate %d bytes for method...", message->method.len + 1), __FILE__, __LINE__);
			}

			memcpy(pszMethod, message->method.p, message->method.len);
			pszMethod[message->method.len] = 0;

			pszURI = (char *)malloc(message->uri.len + 1);

			if (pszURI == NULL) {
				throw wctl_error(wctl_error::buildMsg("Failed to allocate %d bytes for URI...", message->uri.len + 1), __FILE__, __LINE__);
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
	const char *		pszToken;

	ConfigManager & cfg = ConfigManager::getInstance();

	pszToken = cfg.getValue("admin.docroot");
	strcpy(this->szHTMLDocRoot, pszToken);
	strcat(this->szHTMLDocRoot, "/html");
	strcpy(this->szCSSDocRoot, pszToken);
}

WebAdmin::~WebAdmin()
{
}

void WebAdmin::initListener()
{
	char				szPort[16];
	struct mg_bind_opts opts;

	ConfigManager & cfg = ConfigManager::getInstance();
	Logger & log = Logger::getInstance();
	
	memset(&opts, 0, sizeof(mg_bind_opts));

	strcpy(szPort, cfg.getValue("admin.listenport"));

	mg_mgr_init(&mgr, NULL);

	log.logStatus("Setting up listener on port %s", szPort);

#ifdef MG_ENABLE_SSL
	if (cfg.getValueAsBoolean("admin.issecure")) {
		opts.ssl_cert = cfg.getValue("admin.sslcert");
		opts.ssl_key = cfg.getValue("admin.sslkey");
	}
#endif

	connection = mg_bind_opt(
					&mgr, 
					szPort, 
					nullHandler,
					opts);

	if (connection == NULL) {
		log.logError("Failed to bind to port %s", szPort);
		throw wctl_error(wctl_error::buildMsg("Faled to bind to port %s", szPort), __FILE__, __LINE__);
	}

	log.logStatus("Bound default handler to %s...", szPort);

	mg_set_protocol_http_websocket(connection);
}

void WebAdmin::listen()
{
	Logger & log = Logger::getInstance();

	log.logStatus("Listening...");

	while (1) {
		mg_mgr_poll(&mgr, 1000);
	}

	mg_mgr_free(&mgr);
}

void WebAdmin::registerHandler(const char * pszURI, void (* handler)(struct mg_connection *, int, void *))
{
	mg_register_http_endpoint(connection, pszURI, handler);
}
