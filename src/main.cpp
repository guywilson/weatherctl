/******************************************************************************
**
** Main WCTL file...
**
** Contains the program entry point
**
******************************************************************************/
#include <iostream>
#include <cstring>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>

#include "serial.h"
#include "wctl_error.h"
#include "queuemgr.h"
#include "currenttime.h"
#include "webadmin.h"
#include "backup.h"
#include "logger.h"
#include "configmgr.h"
#include "posixthread.h"
#include "threads.h"
#include "views.h"
#include "wctl.h"

extern "C" {
#include "strutils.h"
}

#include "version.h"

using namespace std;

void printUsage(char * pszAppName)
{
	printf("\n Usage: %s [OPTIONS]\n\n", pszAppName);
	printf("  Options:\n");
	printf("   -h/?             Print this help\n");
	printf("   -version         Print the program version\n");
	printf("   -port device     Serial port device\n");
	printf("   -baud baudrate   Serial port baud rate\n");
	printf("   -cfg configfile  Specify the cfg file, default is ./webconfig.cfg\n");
	printf("   -d               Daemonise this application\n");
	printf("   -log  filename   Write logs to the file\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	FILE *			fptr_pid;
	char *			pszPort = NULL;
	char *			pszBaud = NULL;
	char *			pszAppName;
	char *			pszLogFileName = NULL;
	char *			pszConfigFileName = NULL;
	char			szPidFileName[PATH_MAX];
	int				i;
	bool			isDaemonised = false;
	bool			isAdminOnly = false;
	bool			isAdminEnabled = true;
	bool			isDumpConfig = false;
	char			cwd[PATH_MAX];
	int				defaultLoggingLevel = LOG_LEVEL_INFO | LOG_LEVEL_ERROR | LOG_LEVEL_FATAL;

	CurrentTime::initialiseUptimeClock();
	
	pszAppName = strdup(argv[0]);

    /*
    ** Check privileges...
    */
	if (geteuid() != 0) {
		printf("\n");
		printf("********************************************************************************\n");
        printf("** WARNING!                                                                   **\n");
		printf("**                                                                            **\n");
		printf("** In order to have the ability to reset the AVR device, this software must   **\n");
		printf("** be run as the root user!                                                   **\n");
		printf("**                                                                            **\n");
		printf("********************************************************************************\n");
		printf("\n");
	}

	getcwd(cwd, sizeof(cwd));
	
	strcpy(szPidFileName, cwd);
	strcat(szPidFileName, "/wctl.pid");

	printf("\nRunning %s from %s\n", pszAppName, cwd);

	if (argc > 1) {
		for (i = 1;i < argc;i++) {
			if (argv[i][0] == '-') {
				if (strcmp(&argv[i][1], "port") == 0) {
					pszPort = strdup(&argv[++i][0]);
				}
				else if (strcmp(&argv[i][1], "baud") == 0) {
					pszBaud = strdup(&argv[++i][0]);
				}
				else if (argv[i][1] == 'd') {
					isDaemonised = true;
				}
				else if (strcmp(&argv[i][1], "log") == 0) {
					pszLogFileName = strdup(&argv[++i][0]);
				}
				else if (strcmp(&argv[i][1], "cfg") == 0) {
					pszConfigFileName = strdup(&argv[++i][0]);
				}
				else if (strcmp(&argv[i][1], "-admin-only") == 0) {
					isAdminOnly = true;
				}
				else if (strcmp(&argv[i][1], "-dump-config") == 0) {
					isDumpConfig = true;
				}
				else if (argv[i][1] == 'h' || argv[i][1] == '?') {
					printUsage(pszAppName);
					return 0;
				}
				else if (strcmp(&argv[i][1], "version") == 0) {
					printf("%s Version: [%s], Build date: [%s]\n\n", pszAppName, getVersion(), getBuildDate());
					return 0;
				}
				else {
					printf("Unknown argument '%s'", &argv[i][0]);
					printUsage(pszAppName);
					return 0;
				}
			}
		}
	}
	else {
		printUsage(pszAppName);
		return -1;
	}

	if (isDaemonised) {
		daemonise();
	}

	fptr_pid = fopen(szPidFileName, "wt");
	
	if (fptr_pid == NULL) {
		fprintf(stderr, "Failed top open PID file\n");
		fflush(stderr);
	}
	else {
		fprintf(fptr_pid, "%d\n", getpid());
		fclose(fptr_pid);
	}
	
	openlog(pszAppName, LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started %s", pszAppName);

	ConfigManager & cfg = ConfigManager::getInstance();
	
	try {
		cfg.initialise(pszConfigFileName);
	}
	catch (wctl_error & e) {
		fprintf(stderr, "Could not read config file: %s [%s]\n", pszConfigFileName, e.what());
		fprintf(stderr, "Aborting!\n\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}

	if (pszConfigFileName != NULL) {
		free(pszConfigFileName);
	}

	if (isDumpConfig) {
		cfg.dumpConfig();
	}

	Logger & log = Logger::getInstance();

	if (pszLogFileName != NULL) {
		log.initLogger(pszLogFileName, defaultLoggingLevel);
		free(pszLogFileName);
	}
	else {
		const char * filename = cfg.getValue("log.filename");
		const char * level = cfg.getValue("log.level");

		if (strlen(filename) == 0 && strlen(level) == 0) {
			log.initLogger(defaultLoggingLevel);
		}
		else if (strlen(level) == 0) {
			log.initLogger(filename, defaultLoggingLevel);
		}
		else {
			log.initLogger(filename, level);
		}
	}

	/*
	 * Register signal handler for cleanup...
	 */
	if (signal(SIGINT, &handleSignal) == SIG_ERR) {
		log.logFatal("Failed to register signal handler for SIGINT");
		return -1;
	}

	if (signal(SIGTERM, &handleSignal) == SIG_ERR) {
		log.logFatal("Failed to register signal handler for SIGTERM");
		return -1;
	}

	if (signal(SIGUSR1, &handleSignal) == SIG_ERR) {
		log.logFatal("Failed to register signal handler for SIGUSR1");
		return -1;
	}

	if (signal(SIGUSR2, &handleSignal) == SIG_ERR) {
		log.logFatal("Failed to register signal handler for SIGUSR2");
		return -1;
	}

	if (!isAdminOnly) {
		/*
		* Open the serial port...
		*/
		SerialPort & port = SerialPort::getInstance();

		try {
			if (pszPort != NULL) {
				port.openPort(
						pszPort, 
						SerialPort::mapBaudRate(atoi(pszBaud)), 
						false, 
						false);
			}
			else {
				port.openPort(
						cfg.getValue("serial.port"), 
						SerialPort::mapBaudRate(cfg.getValueAsInteger("serial.baud")), 
						cfg.getValueAsBoolean("serial.isblocking"),
						cfg.getValueAsBoolean("serial.isemulation"));
			}
		}
		catch (wctl_error & e) {
			log.logFatal("Failed to open serial port %s", e.what());
			syslog(LOG_ERR, "Failed to open serial port %s", e.what());
			return -1;
		}

		log.logStatus("Opened serial port...");
	}	

	/*
	** Setup admin server...
	*/
	WebAdmin & web = WebAdmin::getInstance();

	try {
		web.initListener();
	}
	catch (wctl_error & e) {
		log.logError("Failed to initialise admin console, console disabled...");
		isAdminEnabled = false;
	}

	if (isAdminEnabled) {
		web.registerHandler("/", homeViewHandler);

		web.registerHandler("/cmd/cmd.html", avrCmdViewHandler);
		web.registerHandler("/cmd/post", avrCmdCommandHandler);

		web.registerHandler("/calib/calibration.html", avrCalibViewHandler);
		web.registerHandler("/calib/post", avrCalibCommandHandler);

		web.registerHandler("/css", cssHandler);
	}

	if (!isAdminOnly) {
		/*
		** Initialise backup manager...
		*/
		bool isBackupEnabled = cfg.getValueAsBoolean("backup.enabled");
		
		if (isBackupEnabled) {
			BackupManager & backup = BackupManager::getInstance();

			backup.setupCSV(
					cfg.getValue("backup.tph.csv"), 
					cfg.getValue("backup.wind.csv"), 
					cfg.getValue("backup.rain.csv"));
					
			backup.setupPrimaryDB(
				cfg.getValue("backup.primaryhost"), 
				cfg.getValue("backup.primarydb"));
				
			backup.setupSecondaryDB(
				cfg.getValue("backup.secondaryhost"), 
				cfg.getValue("backup.secondarydb"));
		}
	}

	/*
	 * Start threads...
	 */
	ThreadManager & threadMgr = ThreadManager::getInstance();

	threadMgr.startThreads(isAdminOnly, isAdminEnabled);

	while (1) {
		PosixThread::sleep(PosixThread::seconds, 5L);
	}
	
	cleanup();

	return 0;
}
