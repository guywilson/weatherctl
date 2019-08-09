#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "views.h"
#include "exception.h"
#include "avrweather.h"
#include "queuemgr.h"
#include "logger.h"
#include "mongoose.h"

static uint8_t			frame[MAX_REQUEST_MESSAGE_LENGTH];

void avrCommandHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *			message;
	char *							pszMethod;
	char *							pszURI;
	char							szCmdValue[32];
	char							szVersion[64];
	int								rtn;
	uint8_t							cmdCode;
	bool							isSerialCommand = false;
	bool							isRenderable = false;

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

			log.logInfo("Got %s request for '%s'", pszMethod, pszURI);

			if (strncmp(pszMethod, "GET", 4) == 0) {
				rtn = mg_get_http_var(&message->query_string, "command", szCmdValue, 32);

				if (rtn < 0) {
					throw new Exception("Failed to find form variable 'command'");
				}

				log.logInfo("Got command: %s", szCmdValue);

				if (strncmp(szCmdValue, "debug-logging-on", sizeof(szCmdValue)) == 0) {
					int level = log.getLogLevel();
					level |= LOG_LEVEL_DEBUG;
					log.setLogLevel(level);
					isSerialCommand = false;
				}
				else if (strncmp(szCmdValue, "debug-logging-off", sizeof(szCmdValue)) == 0) {
					int level = log.getLogLevel();
					level &= ~LOG_LEVEL_DEBUG;
					log.setLogLevel(level);
					isSerialCommand = false;
				}
				else if (strncmp(szCmdValue, "disable-wd-reset", sizeof(szCmdValue)) == 0) {
					cmdCode = RX_CMD_WDT_DISABLE;
					isSerialCommand = true;
				}
				else if (strncmp(szCmdValue, "reset-min-max", sizeof(szCmdValue)) == 0) {
					cmdCode = RX_CMD_RESET_MINMAX_TPH;
					isSerialCommand = true;
				}
				else if (strncmp(szCmdValue, "get-scheduler-version", sizeof(szCmdValue)) == 0) {
					cmdCode = RX_CMD_GET_SCHED_VERSION;
					isSerialCommand = true;
					isRenderable = true;
				}
				else if (strncmp(szCmdValue, "reset-avr", sizeof(szCmdValue)) == 0) {
					resetAVR();
					isSerialCommand = false;
				}
				else {
					cmdCode = RX_CMD_PING;
					isSerialCommand = true;
				}

				if (isSerialCommand) {
					TxFrame * pTxFrame = new TxFrame(frame, MAX_REQUEST_MESSAGE_LENGTH, NULL, 0, cmdCode);
					
					if (isRenderable) {
						RxFrame * pRxFrame = send_receive(pTxFrame);
						
						memcpy(szVersion, pRxFrame->getData(), pRxFrame->getDataLength());
						szVersion[pRxFrame->getDataLength()] = 0;

						delete pRxFrame;

						log.logInfo("Got scheduler version from Arduino [%s]", szVersion);
						mg_printf(connection, "HTTP/1.1 200 OK\n\n Version [%s]", szVersion);
						connection->flags |= MG_F_SEND_AND_CLOSE;
					}
					else {
						fire_forget(pTxFrame);
					}
				}
			}

			free(pszMethod);
			free(pszURI);

			if (!isRenderable) {
				mg_printf(connection, "HTTP/1.1 200 OK");
				connection->flags |= MG_F_SEND_AND_CLOSE;
			}
			break;

		default:
			break;
	}
}

void avrViewHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *			message;
	char *							pszMethod;
	char *							pszURI;

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

			log.logInfo("Got %s request for '%s'", pszMethod, pszURI);
	
			if (strncmp(pszMethod, "GET", 3) == 0) {
				WebConnector & web = WebConnector::getInstance();

				struct mg_serve_http_opts opts = { .document_root = web.getDocRoot() };

				log.logInfo("Serving file '%s'", pszURI);

				mg_serve_http(connection, message, opts);
			}

			free(pszMethod);
			free(pszURI);

			mg_printf(connection, "HTTP/1.1 200 OK");
			connection->flags |= MG_F_SEND_AND_CLOSE;
			break;

		default:
			break;
	}
}

void cssHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *			message;
	char *							pszMethod;
	char *							pszURI;

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

			log.logInfo("Got %s request for '%s'", pszMethod, pszURI);

			if (strncmp(pszMethod, "GET", 3) == 0) {
				struct mg_serve_http_opts opts = { .document_root = "./resources" };

				log.logInfo("Serving file '%s'", pszURI);

				mg_serve_http(connection, message, opts);
			}

			free(pszMethod);
			free(pszURI);

			mg_printf(connection, "HTTP/1.1 200 OK");
			connection->flags |= MG_F_SEND_AND_CLOSE;
			break;

		default:
			break;
	}
}
