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

#include "serial.h"
#include "exception.h"
#include "queuemgr.h"
#include "avrweather.h"
#include "currenttime.h"
#include "mongoose.h"
#include "webconnect.h"
#include "views.h"
#include "logger.h"

#define LOG_LEVEL			LOG_LEVEL_INFO | LOG_LEVEL_ERROR | LOG_LEVEL_FATAL //| LOG_LEVEL_DEBUG
//#define SERIAL_EMULATION

using namespace std;

pthread_t			tidTxCmd;
int					pid_fd = -1;
char				szAppName[256];

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

int daemonise(char * pszAppName, char * pszPidFileName, char * pszLogFileName)
{
	pid_t pid = 0;

	/* Fork off the parent process */
	pid = fork();

	/* An error occurred */
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}

	/* Success: Let the parent terminate */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* Set new file permissions */
	umask(0);

	/*
	** Open logs...
	*/
	openlog(pszAppName, LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started %s", pszAppName);

	Logger & log = Logger::getInstance();
	log.initLogger(pszLogFileName, LOG_LEVEL);

	/* On success: The child process becomes session leader */
	if (setsid() < 0) {
		exit(EXIT_FAILURE);
	}

	/* Change the working directory to the root directory */
	/* or another appropriated directory */
	chdir("/");

	/* Ignore signal sent from child to parent process */
	signal(SIGCHLD, SIG_IGN);

	/*
	** Close these, as they aren't required...
	*/
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Try to write PID of daemon to lockfile */
	if (pszPidFileName != NULL)
	{
		char str[256];
		pid_fd = open(pszPidFileName, O_RDWR|O_CREAT, 0640);
		if (pid_fd < 0) {
			/* Can't open lockfile */
			exit(EXIT_FAILURE);
		}
		if (lockf(pid_fd, F_TLOCK, 0) < 0) {
			/* Can't lock file */
			exit(EXIT_FAILURE);
		}
		/* Get current PID */
		sprintf(str, "%d\n", getpid());
		/* Write PID to lockfile */
		write(pid_fd, str, strlen(str));
	}

	return 0;
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
	
	if (isDaemonised) {
		if (daemonise(szAppName, pszLockFileName, pszLogFileName) < 0) {
			printf("Error daemonising the process, exiting...\n\n");
			exit(-1);
		}
	}
	else {
		/*
		** Daemonise mode will initialise the logs, otherwise do it here...
		*/
		openlog(szAppName, LOG_PID|LOG_CONS, LOG_DAEMON);
		syslog(LOG_INFO, "Started %s", szAppName);

		log.initLogger(pszLogFileName, LOG_LEVEL);
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

	web.listen();

	log.logInfo("Cleaning up and exiting!");
	
	port.closePort();
	log.closeLogger();
	closelog();

	cleanup();

	return 0;
}
