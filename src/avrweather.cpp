#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "avrweather.h"
#include "queuemgr.h"
#include "exception.h"
#include "calibration.h"
#include "postdata.h"
#include "currenttime.h"
#include "logger.h"
#include "posixthread.h"

using namespace std;

#define CPU_HISTORY_SIZE			16
#define TPH_MSGS_PER_MINUTE			3

float		cpuHistory[CPU_HISTORY_SIZE];
int			cpuIndex = 0;

void getAVRCpuHistory(float * historyArray, int arraySize)
{
	int			i = 0;
	int			j = 0;

	if (cpuIndex == CPU_HISTORY_SIZE) {
		j = 0;
	}
	else {
		j = cpuIndex + 1;
	}

	while (i < CPU_HISTORY_SIZE && i < arraySize) {
		historyArray[i++] = cpuHistory[j++];

		if (j == CPU_HISTORY_SIZE) {
			j = 0;
		}
	}
}

float getAVRCpuAverage()
{
	float		avg = 0.0;
	int			i;
	
	for (i = 0;i < CPU_HISTORY_SIZE;i++) {
		avg += cpuHistory[i];
	}

	avg = avg / CPU_HISTORY_SIZE;

	return avg;
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
		PosixThread::sleep(10L);
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
	double		offset;
	double		factor;

	CalibrationData & cd = CalibrationData::getInstance();

	offset = cd.getOffset(cd.thermometer);
	factor = cd.getFactor(cd.thermometer);

	temperature = 
		((((((double)sensorValue + offset) / (double)1023) * (double)5) - (double)1.375) / (double)0.0225) * factor;

	return temperature;
}

/*
** Pmbar = (((adc / 1023) + 0.095) / 0.009) * 10
*/
double getActualPressure(uint16_t sensorValue)
{
	double		pressure;
	double		offset;
	double		factor;

	CalibrationData & cd = CalibrationData::getInstance();

	offset = cd.getOffset(cd.barometer);
	factor = cd.getFactor(cd.barometer);

	pressure = 
		((((((double)sensorValue + offset) / (double)1023) + (double)0.095) / (double)0.009) * (double)10) * factor;

	return pressure;
}

/*
** RH = ((ADC / 1023) - 0.16) / 0.0062
*/
double getActualHumidity(uint16_t sensorValue)
{
	double		humidity;
	double		offset;
	double		factor;

	CalibrationData & cd = CalibrationData::getInstance();

	offset = cd.getOffset(cd.hygrometer);
	factor = cd.getFactor(cd.hygrometer);

	humidity = 
		(((((double)sensorValue + offset) / (double)1023) - (double)0.16) / (double)0.0062) * factor;

	return humidity;
}

double getActualWindspeed(uint16_t sensorValue)
{
	double		windspeed;
	double		offset;
	double		factor;

	CalibrationData & cd = CalibrationData::getInstance();

	offset = cd.getOffset(cd.anemometer);
	factor = cd.getFactor(cd.anemometer);

	windspeed = ((double)sensorValue + offset) * RPS_TO_KPH_SCALE_FACTOR * factor;

	return windspeed;
}

double getActualRainfall(uint16_t sensorValue)
{
	double		rainfall;
	double		offset;
	double		factor;

	CalibrationData & cd = CalibrationData::getInstance();

	offset = cd.getOffset(cd.rainGauge);
	factor = cd.getFactor(cd.rainGauge);

	rainfall = ((double)sensorValue + offset) * TIPS_TO_MM_SCALE_FACTOR * factor;
	
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
				if (isprint(b)) {
					log.logDebugNoCR("%c", b);
				}
				else {
					log.logDebugNoCR("[%02X]", b);
				}
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
	float				cpuPct;
	
	TPH_PACKET 			tph;
	WINDSPEED			ws;
	RAINFALL			rf;
	CPU_RATIO			cpu;
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
			case RX_RSP_TPH:
				log.logDebug("TPH - Copying %d bytes", pFrame->getDataLength());
				
				memcpy(&tph, pFrame->getData(), pFrame->getDataLength());

				delete pFrame;

				log.logDebug(
					"Got TPH data (min, avg, max): T = %d:%d:%d, P = %d:%d:%d, H = %d:%d:%d", 
					tph.min.temperature,
					tph.current.temperature,
					tph.max.temperature,
					tph.min.pressure,
					tph.current.pressure,
					tph.max.pressure, 
					tph.min.humidity,
					tph.current.humidity,
					tph.max.humidity);

				qmgr.pushWebPost(new PostDataTPH("AVG", avgSave, &tph.current));
				qmgr.pushWebPost(new PostDataTPH("MAX", maxSave, &tph.max));
				qmgr.pushWebPost(new PostDataTPH("MIN", minSave, &tph.min));
				
				maxSave = false;
				minSave = false;

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

			case RX_RSP_RESET_MINMAX:
				log.logInfo("AVR has successfully rest min/max values...");
				delete pFrame;
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
				delete pFrame;
				break;

			case RX_RSP_GET_DASHBOARD:
				log.logInfo("Got dashboard response from AVR...");
				qmgr.pushRx(pFrame);
				break;

			case RX_RSP_CPU_PERCENTAGE:
				log.logDebug("CPU pct - Copying %d bytes", pFrame->getDataLength());

				memcpy(&cpu, pFrame->getData(), pFrame->getDataLength());

				delete pFrame;

				cpuPct = ((float)cpu.busyCount / (float)cpu.idleCount) * 100.0;
				cpuHistory[cpuIndex++] = cpuPct;

				if (cpuIndex == CPU_HISTORY_SIZE) {
					cpuIndex = 0;
				}

				log.logDebug("Got AVR CPU percentage - %.3f", cpuPct);
				break;

			case RX_RSP_PING:
				delete pFrame;
				break;
		}
	}
	else {
		log.logError("NAK received with error code [0x%02X] for response [0x%04X]", pFrame->getErrorCode(), pFrame->getResponseCode());
		delete pFrame;
	}
}
