#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "views.h"
#include "webadmin.h"
#include "exception.h"
#include "avrweather.h"
#include "queuemgr.h"
#include "logger.h"
#include "postgres.h"
#include "mongoose.h"
#include "html_template.h"

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

void avrCmdCommandHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *			message;
	char *							pszMethod;
	char *							pszURI;
	char							szCmdValue[32];
	char							szRenderBuffer[256];
	uint8_t							data[80];
	uint8_t *						pData = NULL;
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
				else if (strncmp(szCmdValue, "get-calibration-data", sizeof(szCmdValue)) == 0) {
					cmdCode = RX_CMD_GET_CALIBRATION_DATA;
					isSerialCommand = true;
					isRenderable = true;
				}
				else if (strncmp(szCmdValue, "set-calibration-data", sizeof(szCmdValue)) == 0) {
					cmdCode = RX_CMD_SET_CALIBRATION_DATA;
					isSerialCommand = true;
					isRenderable = false;

					pData = data;
					dataLength = sizeof(CALIBRATION_DATA);

					CALIBRATION_DATA cd;
					cd.thermometerOffset = -5;
					cd.barometerOffset = 4;
					cd.humidityOffset = 0;
					cd.anemometerOffset = -3;
					cd.rainGaugeOffset = 2;

					memcpy(pData, &cd, dataLength);
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
						else if (pRxFrame->getResponseCode() == RX_RSP_GET_CALIBRATION_DATA) {
							CALIBRATION_DATA cd;

							memcpy(&cd, pRxFrame->getData(), pRxFrame->getDataLength());

							sprintf(
								szRenderBuffer,
								"Calibration Data:\n\tTemperature: %d,\n\tPressure: %d,\n\tHumidity: %d,\n\tWindspeed: %d,\n\tRainfall: %d\n",
								cd.thermometerOffset,
								cd.barometerOffset,
								cd.humidityOffset,
								cd.anemometerOffset,
								cd.rainGaugeOffset);

							log.logInfo("Got calibration data from Arduino %s", szRenderBuffer);
							mg_printf(connection, "HTTP/1.1 200 OK\n\n %s", szRenderBuffer);
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
	char							szUpdateStatement[512];
	CALIBRATION_DATA				cdata;
	int								rtn;

	Logger & log = Logger::getInstance();

	switch (event) {
		case MG_EV_HTTP_REQUEST:
			message = (struct http_message *)p;

			pszMethod = getMethod(message);
			pszURI = getURI(message);

			log.logInfo("Got %s request for '%s'", pszMethod, pszURI);

			if (strncmp(pszMethod, "GET", 4) == 0) {
				Postgres pg("localhost", 5432, "data", "guy", "password");

				rtn = mg_get_http_var(&message->query_string, "thermometer-offset", szValue, 32);
				cdata.thermometerOffset = atoi(szValue);

				rtn = mg_get_http_var(&message->query_string, "thermometer-factor", szValue, 32);
				cdata.thermometerFactor = atof(szValue);

				sprintf(
					szUpdateStatement, 
					"UPDATE calibration SET OFFSET_AMOUNT = '%d', FACTOR = '%.3f' WHERE NAME = 'thermometer'",
					cdata.thermometerOffset,
					cdata.thermometerFactor);

				pg.UPDATE(szUpdateStatement);

				rtn = mg_get_http_var(&message->query_string, "barometer-offset", szValue, 32);
				cdata.barometerOffset = atoi(szValue);

				rtn = mg_get_http_var(&message->query_string, "barometer-factor", szValue, 32);
				cdata.barometerFactor = atof(szValue);

				sprintf(
					szUpdateStatement, 
					"UPDATE calibration SET OFFSET_AMOUNT = '%d', FACTOR = '%.3f' WHERE NAME = 'barometer'",
					cdata.barometerOffset,
					cdata.barometerFactor);

				pg.UPDATE(szUpdateStatement);

				rtn = mg_get_http_var(&message->query_string, "humidity-offset", szValue, 32);
				cdata.humidityOffset = atoi(szValue);

				rtn = mg_get_http_var(&message->query_string, "humidity-factor", szValue, 32);
				cdata.humidityFactor = atof(szValue);

				sprintf(
					szUpdateStatement, 
					"UPDATE calibration SET OFFSET_AMOUNT = '%d', FACTOR = '%.3f' WHERE NAME = 'humidity'",
					cdata.humidityOffset,
					cdata.humidityFactor);

				pg.UPDATE(szUpdateStatement);

				rtn = mg_get_http_var(&message->query_string, "anemometer-offset", szValue, 32);
				cdata.anemometerOffset = atoi(szValue);

				rtn = mg_get_http_var(&message->query_string, "anemometer-factor", szValue, 32);
				cdata.anemometerFactor = atof(szValue);

				sprintf(
					szUpdateStatement, 
					"UPDATE calibration SET OFFSET_AMOUNT = '%d', FACTOR = '%.3f' WHERE NAME = 'anemometer'",
					cdata.anemometerOffset,
					cdata.anemometerFactor);

				pg.UPDATE(szUpdateStatement);

				rtn = mg_get_http_var(&message->query_string, "rain-offset", szValue, 32);
				cdata.rainGaugeOffset = atoi(szValue);

				rtn = mg_get_http_var(&message->query_string, "rain-factor", szValue, 32);
				cdata.rainGaugeFactor = atof(szValue);

				sprintf(
					szUpdateStatement, 
					"UPDATE calibration SET OFFSET_AMOUNT = '%d', FACTOR = '%.3f' WHERE NAME = 'rain'",
					cdata.rainGaugeOffset,
					cdata.rainGaugeFactor);

				pg.UPDATE(szUpdateStatement);
				
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

void avrCmdViewHandler(struct mg_connection * connection, int event, void * p)
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
				WebAdmin & web = WebAdmin::getInstance();

				struct mg_serve_http_opts opts = { .document_root = web.getHTMLDocRoot() };

				log.logInfo("Serving file '%s'", pszURI);

				mg_serve_http(connection, message, opts);
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
	char							szCalibrationData[32];
	CALIBRATION_DATA				calibrationData;

	Postgres pg("localhost", 5432, "data", "guy", "password");

	Logger & log = Logger::getInstance();

	switch (event) {
		case MG_EV_HTTP_REQUEST:
			message = (struct http_message *)p;

			pszMethod = getMethod(message);
			pszURI = getURI(message);

			log.logInfo("Got %s request for '%s'", pszMethod, pszURI);
	
			if (strncmp(pszMethod, "GET", 3) == 0) {
				WebAdmin & web = WebAdmin::getInstance();

				struct mg_serve_http_opts opts = { .document_root = web.getHTMLDocRoot() };

				log.logInfo("Serving file '%s'", pszURI);

				string htmlFileName(web.getHTMLDocRoot());
				htmlFileName.append(pszURI);

				string templateFileName(htmlFileName);
				templateFileName.append(".template");

				tmpl::html_template templ(templateFileName);

				pg.getCalibrationData(&calibrationData);

				sprintf(szCalibrationData, "%d", calibrationData.thermometerOffset);
				templ("temperature-offset") = szCalibrationData;

				sprintf(szCalibrationData, "%.3f", calibrationData.thermometerFactor);
				templ("temperature-factor") = szCalibrationData;

				sprintf(szCalibrationData, "%d", calibrationData.barometerOffset);
				templ("pressure-offset") = szCalibrationData;

				sprintf(szCalibrationData, "%.3f", calibrationData.barometerFactor);
				templ("pressure-factor") = szCalibrationData;

				sprintf(szCalibrationData, "%d", calibrationData.humidityOffset);
				templ("humidity-offset") = szCalibrationData;

				sprintf(szCalibrationData, "%.3f", calibrationData.humidityFactor);
				templ("humidity-factor") = szCalibrationData;

				sprintf(szCalibrationData, "%d", calibrationData.anemometerOffset);
				templ("wind-offset") = szCalibrationData;

				sprintf(szCalibrationData, "%.3f", calibrationData.anemometerFactor);
				templ("wind-factor") = szCalibrationData;

				sprintf(szCalibrationData, "%d", calibrationData.rainGaugeOffset);
				templ("rain-offset") = szCalibrationData;

				sprintf(szCalibrationData, "%.3f", calibrationData.rainGaugeFactor);
				templ("rain-factor") = szCalibrationData;

				templ.Process();

				fstream fs;
				fs.open(htmlFileName, ios::out);
				fs << templ;
				fs.close();

				mg_serve_http(connection, message, opts);
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
				WebAdmin & web = WebAdmin::getInstance();

				struct mg_serve_http_opts opts = { .document_root = web.getCSSDocRoot() };

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
