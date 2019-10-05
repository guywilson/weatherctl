#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "views.h"
#include "webadmin.h"
#include "exception.h"
#include "avrweather.h"
#include "calibration.h"
#include "queuemgr.h"
#include "logger.h"
#include "mongoose.h"
#include "html_template.h"

extern "C" {
#include "strutils.h"
}

using namespace std;

static char * getMethod(struct http_message * message)
{
	char *				pszMethod;

	pszMethod = (char *)malloc(message->method.len + 1);

	if (pszMethod == NULL) {
		throw new Exception("Failed to allocate memory for method...");
	}

	memcpy(pszMethod, message->method.p, message->method.len);
	pszMethod[message->method.len] = 0;

	return pszMethod;
}

static char * getURI(struct http_message * message)
{
	char *				pszURI;

	pszURI = (char *)malloc(message->uri.len + 1);

	if (pszURI == NULL) {
		throw new Exception("Failed to allocate memory for URI...");
	}

	memcpy(pszURI, message->uri.p, message->uri.len);
	pszURI[message->uri.len] = 0;

	return pszURI;
}

static int authenticate(struct mg_connection * connection, struct http_message * message)
{
	FILE *		fpDigest;

	ConfigManager & cfg = ConfigManager::getInstance();
	Logger & log = Logger::getInstance();

	fpDigest = fopen(cfg.getValueAsCstr("admin.authfile"), "r");

	if (fpDigest == NULL) {
		log.logError("Failed to open auth file %s", cfg.getValueAsCstr("admin.authfile"));
		throw new Exception("Failed to open auth file");
	}

	if (!mg_http_check_digest_auth(message, "weatherctl", fpDigest)) {
		mg_http_send_digest_auth_request(connection, "weatherctl");
	}

	fclose(fpDigest);
	
	return 0;
}

static struct mg_serve_http_opts getHTMLOpts()
{
	struct mg_serve_http_opts opts;

	ConfigManager & cfg = ConfigManager::getInstance();
	WebAdmin & web = WebAdmin::getInstance();

	opts.document_root = web.getHTMLDocRoot();
	opts.enable_directory_listing = "no";
	opts.global_auth_file = cfg.getValueAsCstr("admin.authfile");

	return opts;
}

static struct mg_serve_http_opts getCSSOpts()
{
	ConfigManager & cfg = ConfigManager::getInstance();
	WebAdmin & web = WebAdmin::getInstance();

	static struct mg_serve_http_opts opts = {
		.document_root = web.getCSSDocRoot(), 
		.enable_directory_listing = "no",
		.global_auth_file =  cfg.getValueAsCstr("admin.authfile")};

	return opts;
}

void avrCmdCommandHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *			message;
	char *							pszMethod;
	char *							pszURI;
	uint8_t *						pData = NULL;
	char							szCmdValue[32];
	char							szRenderBuffer[256];
	int								dataLength = 0;
	int								rtn;
	uint8_t							cmdCode;
	bool							isSerialCommand = false;
	bool							isRenderable = false;

	Logger & log = Logger::getInstance();

	switch (event) {
		case MG_EV_HTTP_REQUEST:
			message = (struct http_message *)p;

			pszMethod = getMethod(message);
			pszURI = getURI(message);

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
					cmdCode = RX_CMD_RESET_MINMAX;
					isSerialCommand = true;
				}
				else if (strncmp(szCmdValue, "get-wctl-version", sizeof(szCmdValue)) == 0) {
					sprintf(szRenderBuffer, "WCTL Version: %s [%s]", getVersion(), getBuildDate());
					isSerialCommand = false;
					isRenderable = true;
				}
				else if (strncmp(szCmdValue, "get-avr-version", sizeof(szCmdValue)) == 0) {
					cmdCode = RX_CMD_GET_AVR_VERSION;
					isSerialCommand = true;
					isRenderable = true;
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
					TxFrame * pTxFrame = new TxFrame(pData, dataLength, cmdCode);
					
					if (isRenderable) {
						RxFrame * pRxFrame = send_receive(pTxFrame);

						if (pRxFrame->getResponseCode() == RX_RSP_GET_SCHED_VERSION) {
							memcpy(szRenderBuffer, pRxFrame->getData(), pRxFrame->getDataLength());
							szRenderBuffer[pRxFrame->getDataLength()] = 0;

							log.logInfo("Got scheduler version from Arduino [%s]", szRenderBuffer);
							mg_printf(connection, "HTTP/1.1 200 OK\n\n Scheduler Version [%s]", szRenderBuffer);
						}
						else if (pRxFrame->getResponseCode() == RX_RSP_GET_AVR_VERSION) {
							memcpy(szRenderBuffer, pRxFrame->getData(), pRxFrame->getDataLength());
							szRenderBuffer[pRxFrame->getDataLength()] = 0;

							log.logInfo("Got avr version from Arduino [%s]", szRenderBuffer);
							mg_printf(connection, "HTTP/1.1 200 OK\n\n AVR Version: %s", szRenderBuffer);
						}
						else {
							mg_printf(connection, "HTTP/1.1 200 OK");
						}

						delete pRxFrame;
					}
					else {
						fire_forget(pTxFrame);
						mg_printf(connection, "HTTP/1.1 200 OK");
					}
				}
				else {
					if (isRenderable) {
						mg_printf(connection, "HTTP/1.1 200 OK\n\n %s", szRenderBuffer);
					}
					else {
						mg_printf(connection, "HTTP/1.1 200 OK");
					}
				}
			}

			free(pszMethod);
			free(pszURI);

			connection->flags |= MG_F_SEND_AND_CLOSE;
			break;

		default:
			break;
	}
}

void avrCalibCommandHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *			message;
	char *							pszMethod;
	char *							pszURI;
	char							szValue[32];

	Logger & log = Logger::getInstance();

	switch (event) {
		case MG_EV_HTTP_REQUEST:
			message = (struct http_message *)p;

			pszMethod = getMethod(message);
			pszURI = getURI(message);

			log.logInfo("Got %s request for '%s'", pszMethod, pszURI);

			if (strncmp(pszMethod, "GET", 4) == 0) {
				CalibrationData & cd = CalibrationData::getInstance();

				mg_get_http_var(&message->query_string, "thermometer-offset", szValue, 32);
				cd.setOffset(cd.thermometer, szValue);

				mg_get_http_var(&message->query_string, "thermometer-factor", szValue, 32);
				cd.setFactor(cd.thermometer, szValue);

				mg_get_http_var(&message->query_string, "barometer-offset", szValue, 32);
				cd.setOffset(cd.barometer, szValue);

				mg_get_http_var(&message->query_string, "barometer-factor", szValue, 32);
				cd.setFactor(cd.barometer, szValue);

				mg_get_http_var(&message->query_string, "hygrometer-offset", szValue, 32);
				cd.setOffset(cd.hygrometer, szValue);

				mg_get_http_var(&message->query_string, "hygrometer-factor", szValue, 32);
				cd.setFactor(cd.hygrometer, szValue);

				mg_get_http_var(&message->query_string, "anemometer-offset", szValue, 32);
				cd.setOffset(cd.anemometer, szValue);

				mg_get_http_var(&message->query_string, "anemometer-factor", szValue, 32);
				cd.setFactor(cd.anemometer, szValue);

				mg_get_http_var(&message->query_string, "raingauge-offset", szValue, 32);
				cd.setOffset(cd.rainGauge, szValue);

				mg_get_http_var(&message->query_string, "raingauge-factor", szValue, 32);
				cd.setFactor(cd.rainGauge, szValue);

				cd.save();

				mg_http_send_redirect(
								connection, 
								302, 
								mg_mk_str("/calib/calibration.html"), 
								mg_mk_str(NULL));
			}

			free(pszMethod);
			free(pszURI);

			connection->flags |= MG_F_SEND_AND_CLOSE;
			break;

		default:
			break;
	}
}

void homeViewHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *			message;
	char *							pszMethod;
	char *							pszURI;

	Logger & log = Logger::getInstance();

	switch (event) {
		case MG_EV_HTTP_REQUEST:
			message = (struct http_message *)p;

			authenticate(connection, message);

			pszMethod = getMethod(message);
			pszURI = getURI(message);

			log.logInfo("Got %s request for '%s'", pszMethod, pszURI);
	
			if (strncmp(pszMethod, "GET", 3) == 0) {
				log.logInfo("Serving file '%s'", pszURI);

				mg_serve_http(connection, message, getHTMLOpts());
			}

			free(pszMethod);
			free(pszURI);
			break;

		default:
			break;
	}
}

void avrCmdViewHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *			message;
	char *							pszMethod;
	char *							pszURI;

	Logger & log = Logger::getInstance();

	switch (event) {
		case MG_EV_HTTP_REQUEST:
			message = (struct http_message *)p;

			authenticate(connection, message);

			pszMethod = getMethod(message);
			pszURI = getURI(message);

			log.logInfo("Got %s request for '%s'", pszMethod, pszURI);
	
			if (strncmp(pszMethod, "GET", 3) == 0) {
				log.logInfo("Serving file '%s'", pszURI);

				mg_serve_http(connection, message, getHTMLOpts());
			}

			free(pszMethod);
			free(pszURI);
			break;

		default:
			break;
	}
}

void avrCalibViewHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *			message;
	char *							pszMethod;
	char *							pszURI;

	Logger & log = Logger::getInstance();

	switch (event) {
		case MG_EV_HTTP_REQUEST:
			message = (struct http_message *)p;

			authenticate(connection, message);

			pszMethod = getMethod(message);
			pszURI = getURI(message);

			log.logInfo("Got %s request for '%s'", pszMethod, pszURI);
	
			if (strncmp(pszMethod, "GET", 3) == 0) {
				WebAdmin & web = WebAdmin::getInstance();

				log.logInfo("Serving file '%s'", pszURI);

				string htmlFileName(web.getHTMLDocRoot());
				htmlFileName.append(pszURI);

				string templateFileName(htmlFileName);
				templateFileName.append(".template");

				tmpl::html_template templ(templateFileName);

				CalibrationData & cd = CalibrationData::getInstance();
				cd.retrieve();

				templ("thermometer-offset") = cd.getOffsetAsCStr(cd.thermometer);
				templ("thermometer-factor") = cd.getFactorAsCStr(cd.thermometer);

				templ("barometer-offset") = cd.getOffsetAsCStr(cd.barometer);
				templ("barometer-factor") = cd.getFactorAsCStr(cd.barometer);

				templ("hygrometer-offset") = cd.getOffsetAsCStr(cd.hygrometer);
				templ("hygrometer-factor") = cd.getFactorAsCStr(cd.hygrometer);

				templ("anemometer-offset") = cd.getOffsetAsCStr(cd.anemometer);
				templ("anemometer-factor") = cd.getFactorAsCStr(cd.anemometer);

				templ("raingauge-offset") = cd.getOffsetAsCStr(cd.rainGauge);
				templ("raingauge-factor") = cd.getFactorAsCStr(cd.rainGauge);

				templ.Process();

				fstream fs;
				fs.open(htmlFileName, ios::out);
				fs << templ;
				fs.close();

				mg_serve_http(connection, message, getHTMLOpts());
			}

			free(pszMethod);
			free(pszURI);
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

			pszMethod = getMethod(message);
			pszURI = getURI(message);

			log.logInfo("Got %s request for '%s'", pszMethod, pszURI);

			if (strncmp(pszMethod, "GET", 3) == 0) {
				log.logInfo("Serving file '%s'", pszURI);

				mg_serve_http(connection, message, getCSSOpts());
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
