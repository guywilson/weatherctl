#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <strutils.h>
}

#include "views.h"
#include "webadmin.h"
#include "wctl_error.h"
#include "avrweather.h"
#include "wctl.h"
#include "calibration.h"
#include "queuemgr.h"
#include "logger.h"
#include "mongoose.h"
#include "html_template.h"

extern "C" {
#include "version.h"
}

using namespace std;

static char * getMethod(struct http_message * message)
{
	char *				pszMethod;

	pszMethod = (char *)malloc(message->method.len + 1);

	if (pszMethod == NULL) {
		throw wctl_error(wctl_error::buildMsg("Failed to allocate %d bytes for method...", message->method.len + 1), __FILE__, __LINE__);
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
		throw wctl_error(wctl_error::buildMsg("Failed to allocate %d bytes for URI...", message->uri.len + 1), __FILE__, __LINE__);
	}

	memcpy(pszURI, message->uri.p, message->uri.len);
	pszURI[message->uri.len] = 0;

	return pszURI;
}

static int authenticate(struct mg_connection * connection, struct http_message * message)
{
	FILE *	fpDigest = NULL;

	ConfigManager & cfg = ConfigManager::getInstance();
	Logger & log = Logger::getInstance();

	fpDigest = fopen(cfg.getValue("admin.authfile"), "r");

	if (fpDigest == NULL) {
		log.logError("Failed to open auth file %s: %s", cfg.getValue("admin.authfile"), strerror(errno));
		throw wctl_error(wctl_error::buildMsg("Failed to open auth file %s: %s", cfg.getValue("admin.authfile"), strerror(errno)), __FILE__, __LINE__);
	}

	if (!mg_http_check_digest_auth(message, "weatherctl", fpDigest)) {
		mg_http_send_digest_auth_request(connection, "weatherctl");
	}

	fclose(fpDigest);
	
	return 0;
}

static struct mg_serve_http_opts getHTMLOpts()
{
	static struct mg_serve_http_opts opts;

	ConfigManager & cfg = ConfigManager::getInstance();
	WebAdmin & web = WebAdmin::getInstance();

	opts.document_root = web.getHTMLDocRoot();
	opts.enable_directory_listing = "no";
	opts.global_auth_file = cfg.getValue("admin.authfile");

	return opts;
}

static struct mg_serve_http_opts getCSSOpts()
{
	static struct mg_serve_http_opts opts;

	ConfigManager & cfg = ConfigManager::getInstance();
	WebAdmin & web = WebAdmin::getInstance();

	opts.document_root = web.getCSSDocRoot();
	opts.enable_directory_listing = "no";
	opts.global_auth_file = cfg.getValue("admin.authfile");

	return opts;
}

void avrCmdCommandHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *			message;
	char *							pszMethod;
	char *							pszURI;
	const char *					pszRedirect = "/cmd/cmd.html";
	uint8_t *						pData = NULL;
	char							szCmdValue[32];
	int								dataLength = 0;
	int								rtn;
	uint8_t							cmdCode;
	bool							isSerialCommand = false;

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
					throw wctl_error(wctl_error::buildMsg("Failed to find form variable 'command' for URI %s", pszURI), __FILE__, __LINE__);
				}

				log.logInfo("Got command: %s", szCmdValue);

				if (strncmp(szCmdValue, "debug-logging-on", sizeof(szCmdValue)) == 0) {
					int level = log.getLogLevel();
					level |= (LOG_LEVEL_INFO | LOG_LEVEL_DEBUG);
					log.setLogLevel(level);
					isSerialCommand = false;
					pszRedirect = "/cmd/cmdon.html";
				}
				else if (strncmp(szCmdValue, "debug-logging-off", sizeof(szCmdValue)) == 0) {
					int level = log.getLogLevel();
					level &= ~(LOG_LEVEL_INFO | LOG_LEVEL_DEBUG);
					log.setLogLevel(level);
					isSerialCommand = false;
					pszRedirect = "/cmd/cmd.html";
				}
				else if (strncmp(szCmdValue, "disable-wd-reset", sizeof(szCmdValue)) == 0) {
					cmdCode = RX_CMD_WDT_DISABLE;
					isSerialCommand = true;
				}
				else if (strncmp(szCmdValue, "reset-min-max", sizeof(szCmdValue)) == 0) {
					cmdCode = RX_CMD_RESET_MINMAX;
					isSerialCommand = true;
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
					
					fire_forget(pTxFrame);
				}
			}

			mg_http_send_redirect(
							connection, 
							302, 
							mg_mk_str(pszRedirect), 
							mg_mk_str(NULL));

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
	char							szAVRVersionBuffer[64];
	char							szSchedVersionBuffer[64];
	char							szAVRUptimeBuffer[80];
	char							szNumProcessedMsgsBuffer[32];
	char							szNumTasksRunBuffer[32];
	char							szAVRAvgCPUBuffer[8];
	char							szRpiCoreTemperature[16];
	char *							pszMethod;
	char *							pszURI;
	char 							szWCTLUptimeBuffer[80];					
	const char *					pszWCTLVersion = "";
	const char *					pszWCTLBuildDate = "";
	const char *					pszAVRVersion = "";
	const char *					pszAVRBuildDate = "";
	const char *					pszSchedVersion = "";
	const char *					pszSchedBuildDate = "";
	char *							ref;

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

				if (str_endswith(pszURI, "/") > 0) {
					pszWCTLVersion = getVersion();
					pszWCTLBuildDate = getBuildDate();

					strcpy(szWCTLUptimeBuffer, CurrentTime::getUptime());

					log.logInfo("Got WCTL version %s [%s]", pszWCTLVersion, pszWCTLBuildDate);
					log.logInfo("Got WCTL uptime %s", szWCTLUptimeBuffer);
					
					sprintf(szAVRAvgCPUBuffer, "%.3f%%", getAVRCpuAverage());

					log.logInfo("Got average AVR CPU [%s]", szAVRAvgCPUBuffer);

					sprintf(szRpiCoreTemperature, "%.2fC", getCPUTemp());

					log.logInfo("Got Rpi core temperature [%s]", szRpiCoreTemperature);

					RxFrame * pAVRRxFrame;

					try {
						pAVRRxFrame = send_receive(new TxFrame(NULL, 0, RX_CMD_GET_DASHBOARD));

						if (pAVRRxFrame != NULL) {
							DASHBOARD db;

							memcpy(&db, pAVRRxFrame->getData(), pAVRRxFrame->getDataLength());

							delete pAVRRxFrame;

							strncpy(szAVRVersionBuffer, db.szAVRVersion, sizeof(db.szAVRVersion));

							ref = szAVRVersionBuffer;

							pszAVRVersion = str_trim_trailing(strtok_r(szAVRVersionBuffer, "[]", &ref));
							pszAVRBuildDate = strtok_r(NULL, "[]", &ref);

							log.logInfo("Got avr version from Arduino %s [%s]", pszAVRVersion, pszAVRBuildDate);

							strncpy(szSchedVersionBuffer, db.szSchedVersion, sizeof(db.szSchedVersion));

							ref = szSchedVersionBuffer;

							pszSchedVersion = str_trim_trailing(strtok_r(szSchedVersionBuffer, "[]", &ref));
							pszSchedBuildDate = strtok_r(NULL, "[]", &ref);

							log.logInfo("Got scheduler version from Arduino %s [%s]", pszSchedVersion, pszSchedBuildDate);

							log.logDebug("Uptime received from AVR: %ds", db.uptime);

							strcpy(szAVRUptimeBuffer, CurrentTime::getUptime(db.uptime));

							log.logInfo("Got uptime from Arduino: %s", szAVRUptimeBuffer);
							log.logInfo("Got num processed messaged from Arduino: %d", db.numMessagesProcessed);
							log.logInfo("Got num tasks run from Scheduler: %d", db.numTasksRun);
							
							sprintf(szNumProcessedMsgsBuffer, "%d", db.numMessagesProcessed);
							sprintf(szNumTasksRunBuffer, "%d", db.numTasksRun);
						}
					}
					catch (wctl_error & e) {
						log.logInfo("Timed out waiting for AVR response...");
						pszAVRVersion = "";
						pszAVRBuildDate = "";
						pszSchedVersion = "";
						pszSchedBuildDate = "";
						szAVRUptimeBuffer[0] = 0;
						szNumProcessedMsgsBuffer[0] = 0;
						szNumTasksRunBuffer[0] = 0;
						szRpiCoreTemperature[0] = 0;
					}

					string htmlFileName(web.getHTMLDocRoot());
					htmlFileName.append(pszURI);
					htmlFileName.append("index.html");

					string templateFileName(htmlFileName);
					templateFileName.append(".template");

					log.logDebug("Opening template file [%s]", templateFileName.c_str());

					tmpl::html_template templ(templateFileName);

					templ("wctl-version") = pszWCTLVersion;
					templ("wctl-builddate") = pszWCTLBuildDate;
					templ("wctl-uptime") = szWCTLUptimeBuffer;
					templ("wctl-coretemp") = szRpiCoreTemperature;

					templ("avr-version") = pszAVRVersion;
					templ("avr-builddate") = pszAVRBuildDate;
					templ("avr-uptime") = szAVRUptimeBuffer;
					templ("avr-msgsprocessed") = szNumProcessedMsgsBuffer;
					templ("avr-cpupct") = szAVRAvgCPUBuffer;

					templ("rts-version") = pszSchedVersion;
					templ("rts-builddate") = pszSchedBuildDate;
					templ("rts-tasksrun") = szNumTasksRunBuffer;

					templ.Process();

					fstream fs;
					fs.open(htmlFileName, ios::out);
					fs << templ;
					fs.close();
				}

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
