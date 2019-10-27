#include <stdint.h>
#include <unistd.h>

#include "configmgr.h"
#include "logger.h"
#include "serial.h"
#include "queuemgr.h"
#include "frame.h"
#include "avrweather.h"
#include "exception.h"
#include "webadmin.h"
#include "rest.h"
#include "backup.h"
#include "postdata.h"
#include "threads.h"

extern "C" {
#include "strutils.h"
}

#define TXRX_WAIT_us								25000L

void ThreadManager::startThreads(bool isAdminOnly, bool isAdminEnabled)
{
	Logger & log = Logger::getInstance();

	if (!isAdminOnly) {
		this->pWebPostThread = new WebPostThread();
		if (this->pWebPostThread->start()) {
			log.logInfo("Started WebPostThread successfully");
		}
		else {
			throw new Exception("Failed to start WebPostThread");
		}

		this->pTxCmdThread = new TxCmdThread();
		if (this->pTxCmdThread->start()) {
			log.logInfo("Started TxCmdThread successfully");
		}
		else {
			throw new Exception("Failed to start TxCmdThread");
		}
	}

	if (isAdminEnabled) {
		this->pAdminListenThread = new AdminListenThread();
		if (this->pAdminListenThread->start()) {
			log.logInfo("Started AdminListenThread successfully");
		}
		else {
			throw new Exception("Failed to start AdminListenThread");
		}
	}
}

void ThreadManager::killThreads()
{
	if (this->pAdminListenThread != NULL) {
		this->pAdminListenThread->stop();
	}
	if (this->pWebPostThread != NULL) {
		this->pWebPostThread->stop();
	}
	if (this->pTxCmdThread != NULL) {
		this->pTxCmdThread->stop();
	}
}

int TxCmdThread::getTxRxFrequency()
{
	static int f = 0;

	if (f == 0) {
		ConfigManager & cfg = ConfigManager::getInstance();
		f = cfg.getValueAsInteger("serial.msgfrequency");
	}

	if (f == 0) {
		f = 1;
	}
	
	return f;
}

uint32_t TxCmdThread::getTxRxDelay()
{
	static uint32_t delay = 0;

	if (delay == 0) {
		delay = (1000000L / getTxRxFrequency()) - TXRX_WAIT_us;
	}

	return delay;
}

uint32_t TxCmdThread::getScheduledTime(uint32_t currentCount, int numSeconds)
{
	return (currentCount + (numSeconds * getTxRxFrequency()));
}

void * TxCmdThread::run(void * pArgs)
{
	TxFrame *			pTxFrame;
	uint32_t			txCount = 0;
	uint32_t			txTPH = 0;
	uint32_t			txWindspeed = 3;
	uint32_t			txRainfall = 4;
	uint32_t			txCPURatio = 5;
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

	txResetMinMax = txResetMinMax * getTxRxFrequency();

	QueueMgr & qmgr = QueueMgr::getInstance();

	while (go) {
		if (txCount == txTPH) {
			/*
			** Next TX packet is a request for TPH data...
			*/
			qmgr.pushTx(new TxFrame(NULL, 0, RX_CMD_TPH));

			/*
			** Schedule next tx in 20 seconds...
			*/
			txTPH = getScheduledTime(txCount, 20);
		}
		if (txCount == txWindspeed) {
			/*
			** Next TX packet is a request for windspeed data...
			*/
			qmgr.pushTx(new TxFrame(NULL, 0, RX_CMD_WINDSPEED));

			/*
			** Schedule next tx in 60 seconds...
			*/
			txWindspeed = getScheduledTime(txCount, 60);
		}
		if (txCount == txRainfall) {
			/*
			** Next TX packet is a request for rainfall data...
			*/
			qmgr.pushTx(new TxFrame(NULL, 0, RX_CMD_RAINFALL));

			/*
			** Schedule next tx in 1 hour...
			*/
			txRainfall = getScheduledTime(txCount, 3600);
		}
		if (txCount == txCPURatio) {
			/*
			** Next TX packet is a request for the CPU ratio...
			*/
			qmgr.pushTx(new TxFrame(NULL, 0, RX_CMD_CPU_PERCENTAGE));

			/*
			** Schedule next tx in 10 seconds...
			*/
			txCPURatio = getScheduledTime(txCount, 10);
		}
		if (txCount == txResetMinMax) {
			log.logInfo("Sending reset min/max command to AVR...");
			
			/*
			** Next TX packet is a request to reset min & max values...
			*/
			qmgr.pushTx(new TxFrame(NULL, 0, RX_CMD_RESET_MINMAX));

			/*
			** Schedule next tx in 24 hours...
			*/
			txResetMinMax = getScheduledTime(txCount, 86400);
		}

		/*
		** If there is something in the queue, send it...
		*/
		if (!qmgr.isTxQueueEmpty()) {
			pTxFrame = qmgr.popTx();
		}
		else {
			/*
			** Default is to send a ping...
			*/
			pTxFrame = new TxFrame(NULL, 0, RX_CMD_PING);
		}

		port.setExpectedBytes(7);

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
		** Sleep for 25ms to allow the Arduino to respond...
		*/
		usleep(TXRX_WAIT_us);

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
		** Sleep for the remaining time...
		*/
		usleep(getTxRxDelay());
	}

	return NULL;
}

void * WebPostThread::run(void * pArgs)
{
	int 			rtn = POST_OK;
	int				attempts = 0;
	bool 			go = true;
	bool			doPost = true;
	char			szType[32];
	char *			pszAPIKey;

	Rest 		rest;

	QueueMgr & qmgr = QueueMgr::getInstance();
	Logger & log = Logger::getInstance();
	ConfigManager & cfg = ConfigManager::getInstance();

	PostDataLogin * pPostDataLogin = new PostDataLogin(cfg.getValue("web.email"), cfg.getValue("web.password"));

	try {
		pszAPIKey = rest.login(pPostDataLogin);
	}
	catch (Exception * e) {
		log.logError("Failed to authenticate email %s", cfg.getValue("web.email"));
		return NULL;
	}

	while (go) {
		if (!qmgr.isWebPostQueueEmpty()) {
			PostData * pPostData = qmgr.popWebPost();

			switch (pPostData->getClassID()) {
				case CLASS_ID_TPH:
					log.logDebug("Got TPH post data from queue...");
					strcpy(szType, "TPH");
					break;

				case CLASS_ID_WINDSPEED:
					log.logDebug("Got WINDSPEED post data from queue...");
					strcpy(szType, "wind speed");
					break;

				case CLASS_ID_RAINFALL:
					log.logDebug("Got RAINFALL post data from queue...");
					strcpy(szType, "rainfall");
					break;

				case CLASS_ID_VERSION:
					log.logDebug("Got VERSION post data from queue...");
					strcpy(szType, "version");
					break;

				case CLASS_ID_BASE:
					log.logError("Got BASE post data from queue!");
					break;
			}

			log.logDebug("Posting %s data to %s", szType, rest.getHost());

			attempts = 0;

			doPost = true;

			while (doPost) {
				rtn = rest.post(pPostData, pszAPIKey);

				attempts++;

				BackupManager & backup = BackupManager::getInstance();
				backup.backup(pPostData);

				if (rtn == POST_CURL_ERROR) {
					log.logError("CURL error posting %s data to %s", szType, rest.getHost());
					doPost = false;
				}
				else if (rtn == POST_AUTHENTICATION_ERROR) {
					if (attempts < 2) {
						log.logError("Authentication error posting %s data to %s - Retrying...", szType, rest.getHost());

						/*
						** Login again and get a new key...
						*/
						try {
							pszAPIKey = rest.login(pPostDataLogin);
						}
						catch (Exception * e) {
							log.logError("Failed to re-authenticate email %s", cfg.getValue("web.email"));
							doPost = false;
						}
					}
					else {
						doPost = false;
					}
				}
				else if (rtn == POST_OK) {
					log.logDebug("Successfully posted %s data to %s", szType, rest.getHost());
					doPost = false;
				}
			}

			rtn = POST_OK;

			delete pPostData;
		}

		sleep(1);
	}

	free(pszAPIKey);

	return NULL;
}

void * AdminListenThread::run(void * pArgs)
{
	WebAdmin & web = WebAdmin::getInstance();
	Logger & log = Logger::getInstance();

	web.listen();

	log.logInfo("web.listen returned...");

	return NULL;
}
