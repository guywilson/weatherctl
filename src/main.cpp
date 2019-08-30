#include <iostream>
#include <cstring>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <libpq-fe.h>

#include "serial.h"
#include "exception.h"
#include "queuemgr.h"
#include "avrweather.h"
#include "currenttime.h"
#include "mongoose.h"
#include "webconnect.h"
#include "csvhelper.h"
#include "views.h"
#include "logger.h"

#define LOG_LEVEL			LOG_LEVEL_INFO | LOG_LEVEL_ERROR | LOG_LEVEL_FATAL //| LOG_LEVEL_DEBUG
//#define SERIAL_EMULATION

using namespace std;

pthread_t			tidTxCmd;
pthread_t			tidWebListener;
pthread_t			tidWebPost;
int					pid_fd = -1;
char				szAppName[256];

int insertTPHRecord(const char * pszHost, const char * pszDbName, PostData * pPostData)
{
	char				szConnection[128];
	PGconn *			dbConnection;
	PGresult *			queryResult;
	const char *		pszInsertTemplate;
	char				szInsertStr[128];

	Logger log = Logger::getInstance();

	sprintf(
		szConnection, 
		"host=%s port=5432 dbname=%s password=password", 
		pszHost, 
		pszDbName);

	dbConnection = PQconnectdb(szConnection);

	if (PQstatus(dbConnection) != CONNECTION_OK) {
		log.logError("Cannot connect to remote database [%s]", PQerrorMessage(dbConnection));
		PQfinish(dbConnection);
		return -1;
	}

	queryResult = PQexec(dbConnection, "BEGIN");

	log.logInfo("Opened DB transaction");

	if (PQresultStatus(queryResult) != PGRES_COMMAND_OK) {
		log.logError("Error beginning transaction [%s]", PQerrorMessage(dbConnection));
		PQclear(queryResult);
		PQfinish(dbConnection);
		return -1;
	}

	PQclear(queryResult);

	CurrentTime time;

	pszInsertTemplate= "INSERT INTO TPH (TS, TYPE, TEMPERATURE, PRESSURE, HUMIDITY) VALUES ('%s', '%s', %s, %s, %s)";

	sprintf(
		szInsertStr, 
		pszInsertTemplate, 
		time.getTimeStamp(), 
		pPostData->getType(), 
		pPostData->getTemperature(), 
		pPostData->getPressure(), 
		pPostData->getHumidity());

	queryResult = PQexec(dbConnection, szInsertStr);

	if (PQresultStatus(queryResult) != PGRES_COMMAND_OK) {
		log.logError("Error issuing INSERT statement [%s]", PQerrorMessage(dbConnection));
		PQclear(queryResult);
		PQfinish(dbConnection);
		return -1;
	}
	else {
		log.logInfo("Successfully INSERTed record to database.");
	}

	PQclear(queryResult);

	queryResult = PQexec(dbConnection, "END");
	PQclear(queryResult);

	log.logInfo("Closed DB transaction");

	PQfinish(dbConnection);

	log.logInfo("PQfinish()");

	return 0;
}

void * txCmdThread(void * pArgs)
{
	TxFrame *			pTxFrame;
	uint32_t			txCount = 0;
	uint32_t			txAvgTPH = 0;
	uint32_t			txMinTPH = 1;
	uint32_t			txMaxTPH = 2;
	uint32_t			txResetMinMax;
	int					bytesRead;
	uint8_t				data[MAX_RESPONSE_MESSAGE_LENGTH];
	bool				go = true;
	uint8_t				frame[MAX_REQUEST_MESSAGE_LENGTH];
	int					writeLen;
	int 				i;

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
			pTxFrame = new TxFrame(frame, sizeof(frame), NULL, 0, RX_CMD_AVG_TPH);

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
			pTxFrame = new TxFrame(frame, sizeof(frame), NULL, 0, RX_CMD_MIN_TPH);

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
			pTxFrame = new TxFrame(frame, sizeof(frame), NULL, 0, RX_CMD_MAX_TPH);

			port.setExpectedBytes(30);

			/*
			** Schedule next tx in 20 seconds...
			*/
			txMaxTPH = txCount + 20;
		}
		else if (txCount == txResetMinMax) {
			/*
			** Next TX packet is a request to reset min & max values...
			*/
			pTxFrame = new TxFrame(frame, sizeof(frame), NULL, 0, RX_CMD_RESET_MINMAX_TPH);

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
				pTxFrame = new TxFrame(frame, sizeof(frame), NULL, 0, RX_CMD_PING);

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

void * webListenerThread(void * pArgs)
{
	WebConnector & web = WebConnector::getInstance();
	Logger & log = Logger::getInstance();

	web.listen();

	log.logInfo("web.listen returned...");

	return NULL;
}

void * webPostThread(void * pArgs)
{
	int rtn = 0;
	bool go = true;

	QueueMgr & qmgr = QueueMgr::getInstance();
	WebConnector & web = WebConnector::getInstance();
	Logger & log = Logger::getInstance();

	CurrentTime time;

	while (go) {
		if (!qmgr.isWebPostQueueEmpty()) {
			PostData * pPostData = qmgr.popWebPost();

			if (strcmp(pPostData->getType(), "AVG") == 0) {
				log.logDebug("Posting AVG data...");

				rtn = web.postTPH(
					WEB_PATH_AVG, 
					pPostData->isDoSave(), 
					pPostData->getTemperature(), 
					pPostData->getPressure(), 
					pPostData->getHumidity());
			}
			else if (strcmp(pPostData->getType(), "MAX") == 0) {
				log.logDebug("Posting MAX data...");
				
				rtn = web.postTPH(
					WEB_PATH_MAX, 
					pPostData->isDoSave(), 
					pPostData->getTemperature(), 
					pPostData->getPressure(), 
					pPostData->getHumidity());
			}
			else if (strcmp(pPostData->getType(), "MIN") == 0) {
				log.logDebug("Posting MIN data...");
				
				rtn = web.postTPH(
					WEB_PATH_MIN, 
					pPostData->isDoSave(), 
					pPostData->getTemperature(), 
					pPostData->getPressure(), 
					pPostData->getHumidity());
			}

			if (rtn < 0) {
				log.logError("Error posting to web server");
				log.logInfo("Writing to local CSV instead, you will need to reconcile later...");

				vector<string> record = {
					time.getTimeStamp(), 
					pPostData->getType(), 
					pPostData->getTemperature(), 
					pPostData->getPressure(), 
					pPostData->getHumidity()};

				CSVHelper & csv = CSVHelper::getInstance();

				csv.writeRecord(5, record);
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

void cleanup(void)
{
#ifndef WEB_LISTENER_TEST
	pthread_kill(tidTxCmd, SIGKILL);
#endif

	if (pid_fd != -1) {
		lockf(pid_fd, F_ULOCK, 0);
		close(pid_fd);
	}
}

void handleSignal(int sigNum)
{
	Logger & log = Logger::getInstance();

	switch (sigNum) {
		case SIGINT:
			log.logInfo("Detected SIGINT, cleaning up...");
			break;

		case SIGTERM:
			log.logInfo("Detected SIGTERM, cleaning up...");
			break;
	}

	cleanup();

    exit(0);
}

void printUsage(char * pszAppName)
{
	printf("\n Usage: %s [OPTIONS]\n\n", pszAppName);
	printf("  Options:\n");
	printf("   -h/?             Print this help\n");
	printf("   -port device     Serial port device\n");
	printf("   -baud baudrate   Serial port baud rate\n");
	printf("   -cfg configfile  Specify the cfg file, default is ./webconfig.cfg\n");
	printf("   -d               Daemonise this application\n");
	printf("   -log  filename   Write logs to the file\n");
	printf("   -lock filename   PID lock file (only applicable with -d option))\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	char			szPort[128];
	char			szBaud[8];
	char *			pszLockFileName = NULL;
	char *			pszLogFileName = NULL;
	char *			pszConfigFileName = NULL;
	int				i;
	bool			isDaemonised = false;
	pid_t			pid;
	pid_t			sid;

	strcpy(szAppName, argv[0]);

	if (argc > 1) {
		for (i = 1;i < argc;i++) {
			if (argv[i][0] == '-') {
				if (strcmp(&argv[i][1], "port") == 0) {
					strcpy(szPort, &argv[++i][0]);
				}
				else if (strcmp(&argv[i][1], "baud") == 0) {
					strcpy(szBaud, &argv[++i][0]);
				}
				else if (argv[i][1] == 'd') {
					isDaemonised = true;
				}
				else if (strcmp(&argv[i][1], "lock") == 0) {
					pszLockFileName = strdup(&argv[++i][0]);
				}
				else if (strcmp(&argv[i][1], "log") == 0) {
					pszLogFileName = strdup(&argv[++i][0]);
				}
				else if (strcmp(&argv[i][1], "cfg") == 0) {
					pszConfigFileName = strdup(&argv[++i][0]);
				}
				else if (argv[i][1] == 'h' || argv[i][1] == '?') {
					printUsage(szAppName);
					return 0;
				}
				else {
					printf("Unknown argument '%s'", &argv[i][0]);
					printUsage(szAppName);
					return 0;
				}
			}
		}
	}
	else {
		printUsage(szAppName);
		return -1;
	}

	Logger & log = Logger::getInstance();
	
	openlog(szAppName, LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started %s", szAppName);

	log.initLogger(pszLogFileName, LOG_LEVEL);

	if (isDaemonised) {
		log.logInfo("Starting daemon...");

		do {
			pid = fork();
		}
		while ((pid == -1) && (errno == EAGAIN));

		if (pid < 0) {
			log.logError("Forking daemon failed...");
			exit(EXIT_FAILURE);
		}
		if (pid > 0) {
			log.logInfo("Exiting child process...");
			exit(EXIT_SUCCESS);
		}

		sid = setsid();
		
		if(sid < 0) {
			log.logError("Failed calling setsid()...");
			exit(EXIT_FAILURE);
		}

		signal(SIGCHLD, SIG_IGN);
		signal(SIGHUP, SIG_IGN);    
		
		umask(0);

		if((chdir("/") == -1)) {
			log.logError("Failed changing directory");
			exit(EXIT_FAILURE);
		}

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		/* Try to write PID of daemon to lockfile */
		if (pszLockFileName != NULL)
		{
			char str[256];

			pid_fd = open(pszLockFileName, O_RDWR|O_CREAT, 0640);

			if (pid_fd < 0) {
				log.logError("Failed to open lock file %s", pszLockFileName);
				exit(EXIT_FAILURE);
			}

			if (lockf(pid_fd, F_TLOCK, 0) < 0) {
				log.logError("Failed to lock file...");
				exit(EXIT_FAILURE);
			}

			/* Get current PID */
			sprintf(str, "%d\n", getpid());

			/* Write PID to lockfile */
			write(pid_fd, str, strlen(str));
		}
	}

	/*
	 * Register signal handler for cleanup...
	 */
	if (signal(SIGINT, &handleSignal) == SIG_ERR) {
		log.logError("Failed to register signal handler for SIGINT\n");
		return -1;
	}

	if (signal(SIGTERM, &handleSignal) == SIG_ERR) {
		log.logError("Failed to register signal handler for SIGTERM\n");
		return -1;
	}
	
	/*
	 * Open the serial port...
	 */
	SerialPort & port = SerialPort::getInstance();

#ifdef SERIAL_EMULATION
	port.setEmulationMode();
#endif

	try {
		port.openPort(szPort, SerialPort::mapBaudRate(atoi(szBaud)), false);
	}
	catch (Exception * e) {
		log.logFatal("Failed to open serial port %s", e->getMessage().c_str());
		syslog(LOG_ERR, "Failed to open serial port %s", e->getMessage().c_str());
		return -1;
	}

	log.logInfo("Opened serial port...");

	WebConnector & web = WebConnector::getInstance();

	web.setConfigLocation(pszConfigFileName);
	web.initListener();

	web.registerHandler("/avr/cmd", avrViewHandler);
	web.registerHandler("/avr/cmd/post", avrCommandHandler);
	web.registerHandler("/css", cssHandler);

	/*
	 * Start threads...
	 */
	int	err;

	err = pthread_create(&tidTxCmd, NULL, &txCmdThread, NULL);

	if (err != 0) {
		log.logError("ERROR! Can't create txCmdThread() :[%s]", strerror(err));
		return -1;
	}
	else {
		log.logInfo("Thread txCmdThread() created successfully");
	}

	err = pthread_create(&tidWebListener, NULL, &webListenerThread, NULL);

	if (err != 0) {
		log.logError("ERROR! Can't create webListenerThread() :[%s]", strerror(err));
		return -1;
	}
	else {
		log.logInfo("Thread webListenerThread() created successfully");
	}

	err = pthread_create(&tidWebPost, NULL, &webPostThread, NULL);

	if (err != 0) {
		log.logError("ERROR! Can't create webPostThread() :[%s]", strerror(err));
		return -1;
	}
	else {
		log.logInfo("Thread webPostThread() created successfully");
	}

	while (1) {
		sleep(5);
	}

	log.logInfo("Cleaning up and exiting!");
	
	port.closePort();
	log.closeLogger();
	closelog();

	cleanup();

	return 0;
}
