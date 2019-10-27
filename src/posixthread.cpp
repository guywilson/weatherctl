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

	thread_params * pThreadParms = (thread_params *)pThreadArgs;

	Logger & log = Logger::getInstance();

	while (go) {
		try {
			pThreadRtn = pThreadParms->pThread->run(pThreadParms->pThreadParm);
		}
		catch (Exception * e) {
			log.logError("_threadRunner: Caught exception %s", e->getMessage().c_str());

			if (!pThreadParms->pThread->isRestartable()) {
				go = false;
			}
			else {
				log.logInfo("Restarting thread...");
			}
		}
	}

	return pThreadRtn;
}

PosixThread::PosixThread()
{
	this->_pThreadParams = (thread_params *)malloc(sizeof(thread_params));

	if (this->_pThreadParams == NULL) {
		log.logError("Failed to allocate memory for thread params...");
		throw new Exception("Failed to allocate memory for thread params...");
	}

	this->_pThreadParams->pThread = this;
}

PosixThread::PosixThread(bool isRestartable) : PosixThread()
{
	this->_isRestartable = isRestartable;
}

PosixThread::~PosixThread()
{
	free(this->_pThreadParams);
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

	this->_pThreadParams->pThreadParm = p;

	err = pthread_create(&this->_tid, NULL, &_threadRunner, this->_pThreadParams);

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
