#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "logger.h"
#include "exception.h"
#include "posixthread.h"

static void * _threadRunner(void * pThreadArgs)
{
	void *			pThreadRtn = NULL;
	bool			go = true;

	PosixThread * pThread = (PosixThread *)pThreadArgs;

	Logger & log = Logger::getInstance();

	while (go) {
		try {
			pThreadRtn = pThread->run();
		}
		catch (Exception * e) {
			log.logError("_threadRunner: Caught exception %s", e->getMessage().c_str());
		}

		if (!pThread->isRestartable()) {
			go = false;
		}
		else {
			log.logInfo("Restarting thread...");
			PosixThread::sleep(5);
		}
	}

	return pThreadRtn;
}

PosixThread::PosixThread()
{
}

PosixThread::PosixThread(bool isRestartable) : PosixThread()
{
	this->_isRestartable = isRestartable;
}

PosixThread::~PosixThread()
{
}

void PosixThread::sleep(unsigned long t)
{
	usleep(t * 1000L);
}

bool PosixThread::start()
{
	return this->start(NULL);
}

bool PosixThread::start(void * p)
{
	int			err;

	this->_threadParameters = p;

	err = pthread_create(&this->_tid, NULL, &_threadRunner, this);

	if (err != 0) {
		log.logError("ERROR! Can't create thread :[%s]", strerror(err));
		return false;
	}

	return true;
}

void PosixThread::stop()
{
	pthread_kill(this->_tid, SIGKILL);
}
