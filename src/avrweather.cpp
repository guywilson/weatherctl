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
#include "calibration.h"
#include "postdata.h"
#include "currenttime.h"
#include "logger.h"

using namespace std;

#define TPH_MSGS_PER_MINUTE			3

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
	int				timeout = 50;
	int				waitCount = 0;
	uint8_t			expectedMsgID;

	expectedMsgID = pTxFrame->getMessageID();
	
	QueueMgr & mgr = QueueMgr::getInstance();

	mgr.pushTx(pTxFrame);

	while (mgr.isRxQueueEmpty() && waitCount < timeout) {
		usleep(10000L);
		waitCount++;
	}

	if (waitCount >= timeout) {
		throw new Exception("Timed out waiting for send/receive");
	}
	
	RxFrame * pRxFrame = mgr.popRx();

	while (pRxFrame->getMessageID() != expectedMsgID) {
		/*
		** Queue is empty, push this message back on the queue
		** and return NULL...
		*/
		if (mgr.isRxQueueEmpty()) {
			mgr.pushRx(pRxFrame);
			return NULL;
		}
		
		/*
		** This isn't the message you're looking for...
		** Push it back on the queue and pop the next one.
		*/
		mgr.pushRx(pRxFrame);
		pRxFrame = mgr.popRx();
	}

	return pRxFrame;
}

void fire_forget(TxFrame * pTxFrame)
{
	QueueMgr & mgr = QueueMgr::getInstance();
	mgr.pushTx(pTxFrame);
}

/*
** C = (((ADC / 1023) * 5) - 1.375) / 0.0225
*/
double getActualTemperature(uint16_t sensorValue)
{
	double		temperature;

	CalibrationData & cd = CalibrationData::getInstance();

	sensorValue += cd.getOffset(cd.thermometer);

	temperature = (((((double)sensorValue / (double)1023) * (double)5) - (double)1.375) / (double)0.0225) * cd.getFactor(cd.thermometer);

	return temperature;
}

/*
** Pmbar = (((adc / 1023) + 0.095) / 0.009) * 10
*/
double getActualPressure(uint16_t sensorValue)
{
	double		pressure;

	CalibrationData & cd = CalibrationData::getInstance();

	sensorValue += cd.getOffset(cd.barometer);

	pressure = (((((double)sensorValue / (double)1023) + (double)0.095) / (double)0.009) * (double)10) * cd.getFactor(cd.barometer);

	return pressure;
}

/*
** RH = ((ADC / 1023) - 0.16) / 0.0062
*/
double getActualHumidity(uint16_t sensorValue)
{
	double		humidity;

	CalibrationData & cd = CalibrationData::getInstance();

	sensorValue += cd.getOffset(cd.hygrometer);

	humidity = ((((double)sensorValue / (double)1023) - (double)0.16) / (double)0.0062) * cd.getFactor(cd.hygrometer);

	return humidity;
}

double getActualWindspeed(uint16_t sensorValue)
{
	double		windspeed;

	CalibrationData & cd = CalibrationData::getInstance();

	sensorValue += cd.getOffset(cd.anemometer);

	windspeed = (double)sensorValue * RPS_TO_KPH_SCALE_FACTOR * cd.getFactor(cd.anemometer);

	return windspeed;
}

double getActualRainfall(uint16_t sensorValue)
{
	double		rainfall;

	CalibrationData & cd = CalibrationData::getInstance();

	sensorValue += cd.getOffset(cd.rainGauge);

	rainfall = (double)sensorValue * TIPS_TO_MM_SCALE_FACTOR * cd.getFactor(cd.rainGauge);
	
	return rainfall;
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
	static int			avgCount = 0;
	static int			avgWindspeedCount = 0;
	static int			avgRainfallCount = 0;
	static bool			avgSave = true;
	static bool			minSave = false;
	static bool			maxSave = false;
	static bool			avgWindspeedSave = true;
	static bool			maxWindspeedSave = false;
	static bool			totalRainfallSave = false;
	
	TPH 				tph;
	WINDSPEED			ws;
	RAINFALL			rf;
	CurrentTime 		time;

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
		if (time.getHour() == 23 && time.getMinute() == 59 && time.getSecond() >= 30) {
			maxSave = true;
			minSave = true;
			maxWindspeedSave = true;
			totalRainfallSave = true;
		}

		switch (pFrame->getResponseCode()) {
			case RX_RSP_AVG_TPH:
				log.logDebug("AVG - Copying %d bytes", pFrame->getDataLength());
				
				memcpy(&tph, pFrame->getData(), pFrame->getDataLength());

				delete pFrame;

				log.logDebug(
					"Got AVG data: T = %d, P = %d, H = %d", 
					tph.temperature, 
					tph.pressure, 
					tph.humidity);

				qmgr.pushWebPost(new PostDataTPH("AVG", avgSave, &tph));
				avgCount++;

				/*
				* Save every 20 minutes...
				*/
				if (avgCount < (TPH_MSGS_PER_MINUTE * 20)) {
					avgSave = false;
				}
				else {
					avgSave = true;
					avgCount = 0;
				}
				break;

			case RX_RSP_MAX_TPH:
				log.logDebug("MAX - Copying %d bytes", pFrame->getDataLength());
				
				memcpy(&tph, pFrame->getData(), pFrame->getDataLength());

				delete pFrame;

				log.logDebug(
					"Got MAX data: T = %d, P = %d, H = %d", 
					tph.temperature, 
					tph.pressure, 
					tph.humidity);

				qmgr.pushWebPost(new PostDataTPH("MAX", maxSave, &tph));

				maxSave = false;
				break;

			case RX_RSP_MIN_TPH:
				log.logDebug("MIN - Copying %d bytes", pFrame->getDataLength());
				
				memcpy(&tph, pFrame->getData(), pFrame->getDataLength());

				delete pFrame;

				log.logDebug(
					"Got MIN data: T = %d, P = %d, H = %d", 
					tph.temperature, 
					tph.pressure, 
					tph.humidity);

				qmgr.pushWebPost(new PostDataTPH("MIN", minSave, &tph));

				minSave = false;
				break;

			case RX_RSP_RESET_MINMAX:
				break;

			case RX_RSP_WINDSPEED:
				log.logDebug("Windspeed - Copying %d bytes", pFrame->getDataLength());

				memcpy(&ws, pFrame->getData(), pFrame->getDataLength());

				delete pFrame;

				log.logDebug(
					"Got windspeed data: A = %d, M = %d", 
					ws.avgWindspeed, 
					ws.maxWindspeed);

				qmgr.pushWebPost(new PostDataWindspeed(avgWindspeedSave, maxWindspeedSave, &ws));
				avgWindspeedCount++;

				maxWindspeedSave = false;

				/*
				* Save every 20 minutes...
				*/
				if (avgWindspeedCount < 20) {
					avgWindspeedSave = false;
				}
				else {
					avgWindspeedSave = true;
					avgWindspeedCount = 0;
				}
				break;

			case RX_RSP_RAINFALL:
				log.logDebug("Rainfall - Copying %d bytes", pFrame->getDataLength());

				memcpy(&rf, pFrame->getData(), pFrame->getDataLength());

				delete pFrame;

				log.logDebug(
					"Got rainfall data: A = %d, T = %d", 
					rf.avgRainfall, 
					rf.totalRainfall);

				qmgr.pushWebPost(new PostDataRainfall(true, totalRainfallSave, &rf));
				avgRainfallCount++;

				totalRainfallSave = false;
				break;

			case RX_RSP_WDT_DISABLE:
				break;

			case RX_RSP_GET_SCHED_VERSION:
				qmgr.pushRx(pFrame);
				break;

			case RX_RSP_GET_AVR_VERSION:
				qmgr.pushRx(pFrame);
				break;

			case RX_RSP_GET_CALIBRATION_DATA:
				qmgr.pushRx(pFrame);
				break;

			case RX_RSP_SET_CALIBRATION_DATA:
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
