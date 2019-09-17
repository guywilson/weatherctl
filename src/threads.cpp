#include <stdint.h>
#include <unistd.h>

#include "logger.h"
#include "serial.h"
#include "queuemgr.h"
#include "frame.h"
#include "avrweather.h"
#include "exception.h"
#include "webadmin.h"
#include "rest.h"

extern "C" {
#include "strutils.h"
}

void * txCmdThread(void * pArgs)
{
	TxFrame *			pTxFrame;
	uint32_t			txCount = 0;
	uint32_t			txAvgTPH = 0;
	uint32_t			txMinTPH = 1;
	uint32_t			txMaxTPH = 2;
	uint32_t			txWindspeed = 3;
	uint32_t			txRainfall = 4;
	uint32_t			txResetMinMax;
	int					bytesRead;
	int					writeLen;
	int 				i;
	bool				go = true;
	uint8_t				data[MAX_RESPONSE_MESSAGE_LENGTH];

	Logger & log = Logger::getInstance();

	SerialPort & port = SerialPort::getInstance();

	log.logDebug("Got serial port instance [%u]", &port);

	CurrentTime 		time;

	/*
	** Calculate seconds to midnight...
	*/
	txResetMinMax = ((23 - time.getHour()) * 3600) + ((59 - time.getMinute()) * 60) + (59 - time.getMinute());

	log.logInfo("Seconds till midnight: %u", txResetMinMax);

	while (go) {
		if (txCount == txAvgTPH) {
			/*
			** Next TX packet is a request for TPH data...
			*/
			pTxFrame = new TxFrame(NULL, 0, RX_CMD_AVG_TPH);

			port.setExpectedBytes(30);

			/*
			** Schedule next tx in 20 seconds...
			*/
			txAvgTPH = txCount + 20;
		}
		else if (txCount == txMinTPH) {
			/*
			** Next TX packet is a request for TPH data...
			*/
			pTxFrame = new TxFrame(NULL, 0, RX_CMD_MIN_TPH);

			port.setExpectedBytes(30);

			/*
			** Schedule next tx in 20 seconds...
			*/
			txMinTPH = txCount + 20;
		}
		else if (txCount == txMaxTPH) {
			/*
			** Next TX packet is a request for TPH data...
			*/
			pTxFrame = new TxFrame(NULL, 0, RX_CMD_MAX_TPH);

			port.setExpectedBytes(30);

			/*
			** Schedule next tx in 20 seconds...
			*/
			txMaxTPH = txCount + 20;
		}
		else if (txCount == txWindspeed) {
			/*
			** Next TX packet is a request for windspeed data...
			*/
			pTxFrame = new TxFrame(NULL, 0, RX_CMD_WINDSPEED);

			port.setExpectedBytes(20);

			/*
			** Schedule next tx in 60 seconds...
			*/
			txWindspeed = txCount + 60;
		}
		else if (txCount == txRainfall) {
			/*
			** Next TX packet is a request for rainfall data...
			*/
			pTxFrame = new TxFrame(NULL, 0, RX_CMD_RAINFALL);

			port.setExpectedBytes(20);

			/*
			** Schedule next tx in 1 hour...
			*/
			txRainfall = txCount + 3600;
		}
		else if (txCount == txResetMinMax) {
			/*
			** Next TX packet is a request to reset min & max values...
			*/
			pTxFrame = new TxFrame(NULL, 0, RX_CMD_RESET_MINMAX);

			port.setExpectedBytes(7);

			/*
			** Schedule next tx in 24 hours...
			*/
			txResetMinMax = txCount + 86400;
		}
		else {
			/*
			** If there is something in the queue, send it next...
			*/
			QueueMgr & qmgr = QueueMgr::getInstance();

			if (!qmgr.isTxQueueEmpty()) {
				pTxFrame = qmgr.popTx();

				port.setExpectedBytes(7);
			}
			else {
				/*
				** Default is to send a ping...
				*/
				pTxFrame = new TxFrame(NULL, 0, RX_CMD_PING);

				port.setExpectedBytes(7);
			}
		}

		/*
		** Send cmd frame...
		*/
		try {
			writeLen = port.send(pTxFrame->getFrame(), pTxFrame->getFrameLength());
		}
		catch (Exception * e) {
			log.logError("Error writing to port: %s", e->getMessage().c_str());
			continue;
		}

		if (writeLen < pTxFrame->getFrameLength()) {
			log.logError("ERROR: Written [%d] bytes, but sent [%d] bytes.", writeLen, pTxFrame->getFrameLength());
			continue;
		}

		if (log.isLogLevel(LOG_LEVEL_DEBUG)) {
			log.logDebugNoCR("TX[%d]: ", writeLen);
			for (i = 0;i < writeLen;i++) {
				log.logDebugNoCR("[0x%02X]", pTxFrame->getFrameByteAt(i));
			}
			log.newline();
		}

		delete pTxFrame;

		txCount++;

		/*
		** Sleep for 100ms to allow the Arduino to respond...
		*/
		usleep(100000L);

		/*
		** Read response frame...
		*/
		try {
			log.logDebug("Reading from port");
			bytesRead = port.receive(data, MAX_RESPONSE_MESSAGE_LENGTH);
			log.logDebug("Read %d bytes", bytesRead);
		}
		catch (Exception * e) {
			log.logError("Error reading port: %s", e->getMessage().c_str());
			usleep(900000L);
			continue;
		}

		/*
		** Process response...
		*/
		if (bytesRead) {
			processResponse(data, bytesRead);
		}

		/*
		** Sleep for the remaining 900ms...
		*/
		usleep(900000L);
	}

	return NULL;
}

void * versionPostThread(void * pArgs)
{
	char			szVersionBuffer[80];
	char *			pszAVRVersion;
	char *			pszAVRBuildDate;
	char *			ref;
	const char *	pszWCTLVersion;
	const char *	pszWCTLBuildDate;
	bool			go = true;

	QueueMgr & qmgr = QueueMgr::getInstance();
	Logger & log = Logger::getInstance();

	while (go) {
		RxFrame * pRxFrame = send_receive(new TxFrame(NULL, 0, RX_CMD_GET_AVR_VERSION));

		memcpy(szVersionBuffer, pRxFrame->getData(), pRxFrame->getDataLength());
		szVersionBuffer[pRxFrame->getDataLength()] = 0;

		ref = szVersionBuffer;

		pszAVRVersion = str_trim_trailing(strtok_r(szVersionBuffer, "[]", &ref));
		pszAVRBuildDate = strtok_r(NULL, "[]", &ref);

		delete pRxFrame;

		pszWCTLVersion = getVersion();
		pszWCTLBuildDate = getBuildDate();

		log.logInfo(
			"Posting AVR version '%s [%s]' and WCTL version '%s [%s]'", 
			pszAVRVersion, 
			pszAVRBuildDate, 
			pszWCTLVersion, 
			pszWCTLBuildDate);

		qmgr.pushWebPost(new PostDataVersion(pszWCTLVersion, pszWCTLBuildDate, pszAVRVersion, pszAVRBuildDate));

		/*
		** Sleep for an hour...
		*/
		sleep(3600);
	}

	return NULL;
}

void * webPostThread(void * pArgs)
{
	int rtn = 0;
	bool go = true;

	QueueMgr & qmgr = QueueMgr::getInstance();
	Rest & rest = Rest::getInstance();
	Logger & log = Logger::getInstance();

	while (go) {
		if (!qmgr.isWebPostQueueEmpty()) {
			PostData * pPostData = qmgr.popWebPost();

			switch (pPostData->getClassID()) {
				case CLASS_ID_TPH:
					log.logDebug("Got TPH post data from queue...");
					break;

				case CLASS_ID_WINDSPEED:
					log.logDebug("Got WINDSPEED post data from queue...");
					break;

				case CLASS_ID_RAINFALL:
					log.logDebug("Got RAINFALL post data from queue...");
					break;

				case CLASS_ID_VERSION:
					log.logDebug("Got VERSION post data from queue...");
					break;

				case CLASS_ID_BASE:
					log.logError("Got BASE post data from queue!");
					break;
			}

			log.logDebug("Posting data to %s", rest.getHost());

			rtn = rest.post(pPostData);

			if (rtn < 0) {
				log.logError("Error posting to web server");
			}
			else {
				log.logInfo("Successfully posted to server");
			}

			rtn = 0;

			delete pPostData;
		}

		sleep(1);
	}

	return NULL;
}

void * webListenerThread(void * pArgs)
{
	WebAdmin & web = WebAdmin::getInstance();
	Logger & log = Logger::getInstance();

	web.listen();

	log.logInfo("web.listen returned...");

	return NULL;
}
