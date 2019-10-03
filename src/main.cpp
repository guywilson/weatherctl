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
#include "exception.h"
#include "queuemgr.h"
#include "currenttime.h"
#include "webadmin.h"
#include "backup.h"
#include "logger.h"
#include "configmgr.h"
#include "threads.h"
#include "views.h"

extern "C" {
#include "strutils.h"
}

#include "version.h"

using namespace std;

void cleanup(void)
{
	/*
	** Kill the threads...
	*/
	killThreads();

	/*
	** Close any open DB connection...
	*/
	BackupManager & backup = BackupManager::getInstance();
	backup.close();

	/*
	** Close the serial port...
	*/
	SerialPort & port = SerialPort::getInstance();
	port.closePort();

	/*
	** Close the logger...
	*/
	Logger & log = Logger::getInstance();
	log.logInfo("Cleaning up and exiting...");
	log.closeLogger();

	closelog();
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

		case SIGUSR1:
			/*
			** We're interpreting this as a request to turn on/off debug logging...
			*/
			log.logInfo("Detected SIGUSR1...");

			if (log.isLogLevel(LOG_LEVEL_DEBUG)) {
				int level = log.getLogLevel();
				level &= ~LOG_LEVEL_DEBUG;
				log.setLogLevel(level);
			}
			else {
				int level = log.getLogLevel();
				level |= LOG_LEVEL_DEBUG;
				log.setLogLevel(level);
			}
			return;

		case SIGUSR2:
			/*
			** We're interpreting this as a request to reload config...
			*/
			log.logInfo("Detected SIGUSR2, reloading config...");

			ConfigManager & cfg = ConfigManager::getInstance();
			cfg.readConfig();

			/*
			** The only thing we can change dynamically (at present)
			** is the logging level...
			*/
			log.setLogLevel(cfg.getValueAsCstr("log.level"));
			
			return;
	}

	cleanup();

    exit(0);
}

void daemonise()
{
	pid_t			pid;
	pid_t			sid;

	fprintf(stdout, "Starting daemon...\n");
	fflush(stdout);

	do {
		pid = fork();
	}
	while ((pid == -1) && (errno == EAGAIN));

	if (pid < 0) {
		fprintf(stderr, "Forking daemon failed...\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		fprintf(stdout, "Exiting child process...\n");
		fflush(stdout);
		exit(EXIT_SUCCESS);
	}

	sid = setsid();
	
	if(sid < 0) {
		fprintf(stderr, "Failed calling setsid()...\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}

	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);    
	
	umask(0);

	if((chdir("/") == -1)) {
		fprintf(stderr, "Failed changing directory\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}
	
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

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
	char			cwd[PATH_MAX];
	int				defaultLoggingLevel = LOG_LEVEL_INFO | LOG_LEVEL_ERROR | LOG_LEVEL_FATAL;

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
		exit(EXIT_FAILURE);
	}
	
	fprintf(fptr_pid, "%d\n", getpid());
	
	fclose(fptr_pid);

	openlog(pszAppName, LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started %s", pszAppName);

	ConfigManager & mgr = ConfigManager::getInstance();
	
	mgr.initialise(pszConfigFileName);

	if (pszConfigFileName != NULL) {
		free(pszConfigFileName);
	}

//	mgr.dumpConfig();

	Logger & log = Logger::getInstance();

	if (pszLogFileName != NULL) {
		log.initLogger(pszLogFileName, defaultLoggingLevel);
		free(pszLogFileName);
	}
	else {
		const char * filename = mgr.getValueAsCstr("log.filename");
		const char * level = mgr.getValueAsCstr("log.level");

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
		log.logError("Failed to register signal handler for SIGINT");
		return -1;
	}

	if (signal(SIGTERM, &handleSignal) == SIG_ERR) {
		log.logError("Failed to register signal handler for SIGTERM");
		return -1;
	}

	if (signal(SIGUSR1, &handleSignal) == SIG_ERR) {
		log.logError("Failed to register signal handler for SIGUSR1");
		return -1;
	}

	if (signal(SIGUSR2, &handleSignal) == SIG_ERR) {
		log.logError("Failed to register signal handler for SIGUSR2");
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
						mgr.getValueAsCstr("serial.port"), 
						SerialPort::mapBaudRate(mgr.getValueAsInteger("serial.baud")), 
						mgr.getValueAsBoolean("serial.isblocking"),
						mgr.getValueAsBoolean("serial.isemulation"));
			}
		}
		catch (Exception * e) {
			log.logFatal("Failed to open serial port %s", e->getMessage().c_str());
			syslog(LOG_ERR, "Failed to open serial port %s", e->getMessage().c_str());
			return -1;
		}

		log.logInfo("Opened serial port...");
	}	

	/*
	** Setup admin server...
	*/
	WebAdmin & web = WebAdmin::getInstance();

	web.registerHandler("/", homeViewHandler);

	web.registerHandler("/cmd/cmd.html", avrCmdViewHandler);
	web.registerHandler("/cmd/post", avrCmdCommandHandler);

	web.registerHandler("/calib/calibration.html", avrCalibViewHandler);
	web.registerHandler("/calib/post", avrCalibCommandHandler);

	web.registerHandler("/css", cssHandler);

	if (!isAdminOnly) {
		/*
		** Initialise backup manager...
		*/
		BackupManager & backup = BackupManager::getInstance();

		backup.setupCSV(
				mgr.getValueAsCstr("backup.tph.csv"), 
				mgr.getValueAsCstr("backup.wind.csv"), 
				mgr.getValueAsCstr("backup.rain.csv"));
		backup.setupPrimaryDB(mgr.getValueAsCstr("backup.primaryhost"), mgr.getValueAsCstr("backup.primarydb"));
		backup.setupSecondaryDB(mgr.getValueAsCstr("backup.secondaryhost"), mgr.getValueAsCstr("backup.secondarydb"));
	}

	/*
	 * Start threads...
	 */
	int rtn = startThreads(isAdminOnly);

	if (rtn < 0) {
		return rtn;
	}

	while (1) {
		sleep(5);
	}
	
	cleanup();

	return 0;
}
