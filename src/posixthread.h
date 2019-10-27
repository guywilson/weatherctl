#include <unistd.h>

#include "logger.h"

#ifndef _INCL_POSIXTHREAD
#define _INCL_POSIXTHREAD

class PosixThread;

typedef struct
{
    PosixThread *           pThread;
    void *                  pThreadParm;
}
thread_params;

class PosixThread
{
private:
    pthread_t           _tid;
    bool                _isRestartable = false;
    thread_params *     _pThreadParams = NULL;

    Logger & log = Logger::getInstance();

public:
    PosixThread();
    PosixThread(bool isRestartable);

    ~PosixThread();

    static void         sleep(unsigned long t);

    virtual bool        start();
    virtual bool        start(void * p);

    virtual void        stop();

    virtual pthread_t   getID() {
        return this->_tid;
    }

    virtual bool        isRestartable() {
        return this->_isRestartable;
    }

    virtual void *      run(void * parms) = 0;
};

#endif
