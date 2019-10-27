#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>

extern "C" {
#include <gpioc.h>
}

#include "threads.h"
#include "backup.h"
#include "serial.h"
#include "logger.h"
#include "configmgr.h"

void cleanup(void)
{
	/*
	** Kill the threads...
	*/
	ThreadManager & threadMgr = ThreadManager::getInstance();

	threadMgr.killThreads();

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
			log.setLogLevel(cfg.getValue("log.level"));
			
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
//	close(STDERR_FILENO);
}

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

float getCPUTemp()
{
    float       cpuTemp = -299.0;
#ifdef __arm__
    FILE *      fptr;
    char        szTemp[8];
    int         i = 0;

    ConfigManager & cfg = ConfigManager::getInstance();
    Logger & log = Logger::getInstance();

    fptr = fopen(cfg.getValue("wctl.cputempfile"), "rt");

    if (fptr == NULL) {
        log.logError("Could not open cpu temperature file %s", cfg.getValue("wctl.cputempfile"));
        return 0.0;
    }

    while (!feof(fptr)) {
        szTemp[i++] = (char)fgetc(fptr);
    }
    
    szTemp[i] = 0;

    fclose(fptr);

    cpuTemp = atof(szTemp) / 1000.0;

    log.logDebug("Got Rpi temperature: %.2f", cpuTemp);
#endif

    return cpuTemp;
}
