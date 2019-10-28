#include <unistd.h>

#include "logger.h"

#ifndef _INCL_POSIXTHREAD
#define _INCL_POSIXTHREAD

class PosixThread
{
private:
    pthread_t           _tid;
    bool                _isRestartable = false;
    void *              _threadParameters = NULL;

    Logger & log = Logger::getInstance();

protected:
    virtual void *      getThreadParameters() {
        return this->_threadParameters;
    }

public:
    PosixThread();
    PosixThread(bool isRestartable);

    ~PosixThread();

    /*
    ** Sleep for t milliseconds...
    */
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

    virtual void *      run() = 0;
};

#endif
