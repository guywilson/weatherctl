#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern "C" {
#include <gpioc.h>
}

#include "avrweather.h"
#include "queuemgr.h"
#include "exception.h"
#include "webconnect.h"
#include "currenttime.h"
#include "csvhelper.h"
#include "logger.h"

using namespace std;

#define AVG_MSGS_PER_MIN			3

void resetAVR()
{
#ifdef __arm__
	int		rc = 0;
	int		pin = AVR_RESET_PIN;

    rc = gpioc_open();

    if (rc == 0) {
        gpioc_setPinOutput(pin);

        gpioc_setPinOff(pin);
        usleep(100000);
        gpioc_setPinOn(pin);

        gpioc_close();
    }
#endif
}

RxFrame * send_receive(TxFrame * pTxFrame)
{
	QueueMgr & mgr = QueueMgr::getInstance();

	mgr.pushTx(pTxFrame);

	while (mgr.isRxQueueEmpty()) {
		usleep(100000L);
	}

	RxFrame * pRxFrame = mgr.popRx();

	return pRxFrame;
}

void fire_forget(TxFrame * pTxFrame)
{
	QueueMgr & mgr = QueueMgr::getInstance();
	mgr.pushTx(pTxFrame);
}

void printFrame(uint8_t * buffer, int bufferLength)
{
	int 				state = RX_STATE_START;
	int					i = 0;
	int					count = 0;
	uint8_t				b;

	Logger & log = Logger::getInstance();

	log.logDebugNoCR("RX[%d]: ", bufferLength);
	for (i = 0;i < bufferLength;i++) {
		log.logDebugNoCR("[0x%02X]", buffer[i]);
	}
	log.newline();

	while (count < bufferLength) {
		b = buffer[count++];

		switch (state) {
			case RX_STATE_START:
				log.logDebugNoCR("[S]");
				log.logDebugNoCR("[0x%02X]", b);

				if (b == MSG_CHAR_START) {
					state = RX_STATE_LENGTH;
				}
				break;

			case RX_STATE_LENGTH:
				log.logDebugNoCR("[L]");
				log.logDebugNoCR("[%d]", b);

				state = RX_STATE_MSGID;
				break;

			case RX_STATE_MSGID:
				log.logDebugNoCR("[M]");
				log.logDebugNoCR("[0x%02X]", b);

				state = RX_STATE_RESPONSE;
				break;

			case RX_STATE_RESPONSE:
				log.logDebugNoCR("[R]");
				log.logDebugNoCR("[0x%02X]", b);

				state = RX_STATE_RESPTYPE;
				break;

			case RX_STATE_RESPTYPE:
				if (b == MSG_CHAR_ACK) {
					log.logDebugNoCR("[ACK]");

					if (buffer[1] > 3) {
						state = RX_STATE_DATA;
						i = 0;
					}
					else {
						state = RX_STATE_CHECKSUM;
					}
				}
				else if (b == MSG_CHAR_NAK) {
					log.logDebugNoCR("[NAK]");
					state = RX_STATE_ERRCODE;
				}
				break;

			case RX_STATE_DATA:
				log.logDebugNoCR("%c", b);
				i++;

				if (i == buffer[1] - 3) {
					state = RX_STATE_CHECKSUM;
				}
				break;

			case RX_STATE_ERRCODE:
				log.logDebugNoCR("[E]");
				log.logDebugNoCR("[0x%02X]", b);

				state = RX_STATE_CHECKSUM;
				break;

			case RX_STATE_CHECKSUM:
				log.logDebugNoCR("[C]");
				log.logDebugNoCR("[0x%02X]", b);

				state = RX_STATE_END;
				break;

			case RX_STATE_END:
				log.logDebugNoCR("[N]");
				log.logDebugNoCR("[0x%02X]", b);

				if (b == MSG_CHAR_END) {
					log.newline();
				}

				state = RX_STATE_START;
				break;
		}
	}
}

void processResponse(uint8_t * response, int responseLength)
{
	char				szResponse[MAX_RESPONSE_MESSAGE_LENGTH];
	char 				szTemperature[20];
	char 				szPressure[20];
	char 				szHumidity[20];
	static int			avgCount = 0;
	static bool			avgSave = true;
	static bool			minSave = false;
	static bool			maxSave = false;

	CurrentTime 		time;

	WebConnector & web = WebConnector::getInstance();

	QueueMgr & qmgr = QueueMgr::getInstance();
	RxFrame * pFrame = new RxFrame(response, responseLength);

	Logger & log = Logger::getInstance();

	if (pFrame->isACK() && pFrame->getFrameLength() < (pFrame->getDataLength() + NUM_ACK_RSP_FRAME_BYTES)) {
		log.logInfo(
			"Received %d bytes, but ACK frame should have been %d bytes long - Ignoring...", 
			pFrame->getFrameLength(), 
			(pFrame->getDataLength() + NUM_ACK_RSP_FRAME_BYTES));

		return;
	}

	if (log.isLogLevel(LOG_LEVEL_DEBUG)) {
		printFrame(response, responseLength);
	}

	if (pFrame->isACK()) {
		switch (pFrame->getResponseCode()) {
			case RX_RSP_AVG_TPH:
				log.logDebug("AVG - Copying %d bytes", pFrame->getDataLength());
				memcpy(szResponse, pFrame->getData(), pFrame->getDataLength());

				delete pFrame;

				strcpy(szTemperature, strtok(szResponse, ";"));
				strcpy(szPressure, strtok(NULL, ";"));
				strcpy(szHumidity, strtok(NULL, ";"));

				log.logDebug("Got AVG data: T = %s, P = %s, H = %s", szTemperature, szPressure, szHumidity);

				try {
					web.postTPH(WEB_PATH_AVG, avgSave, &szTemperature[2], &szPressure[2], &szHumidity[2]);
					avgCount++;

					/*
					* Save every 20 minutes...
					*/
					if (avgCount < (AVG_MSGS_PER_MIN * 20)) {
						avgSave = false;
					}
					else {
						avgSave = true;
						avgCount = 0;
					}
				}
				catch (Exception * e) {
					log.logError("Caught exception posting to web server: %s", e->getMessage().c_str());
					log.logError("Writing to local CSV instead");

					vector<string> avgRecord = {time.getTimeStamp(), "AVG", &szTemperature[2], &szPressure[2], &szHumidity[2]};

					CSVHelper & csvAvg = CSVHelper::getInstance();

					csvAvg.writeRecord(5, avgRecord);
				}
				break;

			case RX_RSP_MAX_TPH:
				log.logDebug("MAX - Copying %d bytes", pFrame->getDataLength());
				memcpy(szResponse, pFrame->getData(), pFrame->getDataLength());

				delete pFrame;

				strcpy(szTemperature, strtok(szResponse, ";"));
				strcpy(szPressure, strtok(NULL, ";"));
				strcpy(szHumidity, strtok(NULL, ";"));

				log.logDebug("Got MAX data: T = %s, P = %s, H = %s", szTemperature, szPressure, szHumidity);

				try {
					if (time.getHour() == 23 && time.getMinute() == 59 && time.getSecond() >= 30) {
						maxSave = true;
					}

					web.postTPH(WEB_PATH_MAX, maxSave, &szTemperature[2], &szPressure[2], &szHumidity[2]);

					maxSave = false;
				}
				catch (Exception * e) {
					log.logError("Caught exception posting to web server: %s", e->getMessage().c_str());
					log.logError("Writing to local CSV instead");

					vector<string> maxRecord = {time.getTimeStamp(), "MAX", &szTemperature[2], &szPressure[2], &szHumidity[2]};

					CSVHelper & csvMax = CSVHelper::getInstance();

					csvMax.writeRecord(5, maxRecord);
				}
				break;

			case RX_RSP_MIN_TPH:
				log.logDebug("MIN - Copying %d bytes", pFrame->getDataLength());
				memcpy(szResponse, pFrame->getData(), pFrame->getDataLength());

				delete pFrame;

				strcpy(szTemperature, strtok(szResponse, ";"));
				strcpy(szPressure, strtok(NULL, ";"));
				strcpy(szHumidity, strtok(NULL, ";"));

				log.logDebug("Got MIN data: T = %s, P = %s, H = %s", szTemperature, szPressure, szHumidity);

				try {
					if (time.getHour() == 23 && time.getMinute() == 59 && time.getSecond() >= 30) {
						minSave = true;
					}

					web.postTPH(WEB_PATH_MIN, minSave, &szTemperature[2], &szPressure[2], &szHumidity[2]);

					minSave = false;
				}
				catch (Exception * e) {
					log.logError("Caught exception posting to web server: %s", e->getMessage().c_str());
					log.logError("Writing to local CSV instead");

					vector<string> minRecord = {time.getTimeStamp(), "MIN", &szTemperature[2], &szPressure[2], &szHumidity[2]};

					CSVHelper & csvMin = CSVHelper::getInstance();

					csvMin.writeRecord(5, minRecord);
				}
				break;

			case RX_RSP_RESET_MINMAX_TPH:
				break;

			case RX_RSP_ANEMOMETER:
				break;

			case RX_RSP_RAINGUAGE:
				break;

			case RX_RSP_WDT_DISABLE:
				break;

			case RX_RSP_GET_SCHED_VERSION:
				qmgr.pushRx(pFrame);
				break;

			case RX_RSP_PING:
				break;
		}
	}
	else {
		log.logError("NAK received with error code [0x%02X]", pFrame->getErrorCode());
		delete pFrame;
	}
}
