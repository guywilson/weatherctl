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

#define TXRX_WAIT_ms								25L

void ThreadManager::startThreads(bool isAdminOnly, bool isAdminEnabled)
{
	Logger & log = Logger::getInstance();

	if (!isAdminOnly) {
		this->pWebPostThread = new WebPostThread();
		if (this->pWebPostThread->start()) {
			log.logStatus("Started WebPostThread successfully");
		}
		else {
			throw new Exception("Failed to start WebPostThread");
		}

		this->pTxCmdThread = new TxCmdThread();
		if (this->pTxCmdThread->start()) {
			log.logStatus("Started TxCmdThread successfully");
		}
		else {
			throw new Exception("Failed to start TxCmdThread");
		}

		this->pDataCleanupThread = new DataCleanupThread();
		if (this->pDataCleanupThread->start()) {
			log.logStatus("Started DataCleanupThread successfully");
		}
		else {
			throw new Exception("Failed to start DataCleanupThread");
		}
	}

	if (isAdminEnabled) {
		this->pAdminListenThread = new AdminListenThread();
		if (this->pAdminListenThread->start()) {
			log.logStatus("Started AdminListenThread successfully");
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
		delay = (1000L / getTxRxFrequency()) - TXRX_WAIT_ms;
	}

	return delay;
}

uint32_t TxCmdThread::getScheduledTime(uint32_t currentCount, int numSeconds)
{
	return (currentCount + (numSeconds * getTxRxFrequency()));
}

void * TxCmdThread::run()
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
			log.logStatus("Sending reset min/max command to AVR...");
			
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
		PosixThread::sleep(PosixThread::milliseconds, TXRX_WAIT_ms);

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
			PosixThread::sleep(PosixThread::milliseconds, getTxRxDelay());
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
		PosixThread::sleep(PosixThread::milliseconds, getTxRxDelay());
	}

	return NULL;
}

void * WebPostThread::run()
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

	if (pszAPIKey == NULL) {
		log.logError("Failed to login to web server, exiting...");
		throw new Exception("Failed to login to server - panic!");
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

				case CLASS_ID_CLEANUP:
					log.logDebug("Got CLEANUP post data from queue...");
					strcpy(szType, "cleanup");
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

		PosixThread::sleep(PosixThread::seconds, 1L);
	}

	free(pszAPIKey);

	return NULL;
}

void * AdminListenThread::run()
{
	WebAdmin & web = WebAdmin::getInstance();
	Logger & log = Logger::getInstance();

	web.listen();

	log.logError("web.listen returned...");

	return NULL;
}

void * DataCleanupThread::run()
{
	bool			go = true;
	uint32_t		secondsTillSundayMidnight;

	Logger & log = Logger::getInstance();
	QueueMgr & qmgr = QueueMgr::getInstance();

	CurrentTime 		time;
	Rest				rest;

	/*
	** Calculate seconds to midnight on Sunday...
	*/
	int dow = time.getDayOfWeek() - 1;

	int daysUntilSunday = 7 - dow;

	if (daysUntilSunday == 7) {
		daysUntilSunday = 0;
	}

	secondsTillSundayMidnight = 
		((23 - time.getHour()) * 3600) + ((59 - time.getMinute()) * 60) + (59 - time.getMinute()) + 
		(daysUntilSunday * 3600 * 24);

	log.logStatus("Waiting for %lu seconds until cleanup...", secondsTillSundayMidnight);

//	PosixThread::sleep(PosixThread::seconds, secondsTillSundayMidnight);
	PosixThread::sleep(PosixThread::seconds, 5);

	while (go) {
		log.logStatus("Running cleanup task...");

		qmgr.pushWebPost(new PostDataCleanup());

		/*
		** Sleep for a week...
		*/
		PosixThread::sleep(PosixThread::hours, 24 * 7);
	}
	
	return NULL;
}
