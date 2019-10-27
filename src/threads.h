#include "posixthread.h"

#ifndef _INCL_THREADS
#define _INCL_THREADS

class TxCmdThread : public PosixThread
{
private:
    int         getTxRxFrequency();
    uint32_t    getTxRxDelay();
    uint32_t    getScheduledTime(uint32_t currentCount, int numSeconds);

public:
    TxCmdThread() : PosixThread(true) {}

    void *      run(void * parms);
};

class WebPostThread : public PosixThread
{
public:
    WebPostThread() : PosixThread(true) {}

    void * run(void * parms);
};

class AdminListenThread : public PosixThread
{
public:
    AdminListenThread() : PosixThread(true) {}

    void * run(void * parms);
};

class ThreadManager
{
public:
    static ThreadManager &  getInstance() {
        static ThreadManager instance;
        return instance;
    }

private:
    ThreadManager() {}

    TxCmdThread *           pTxCmdThread = NULL;
    WebPostThread *         pWebPostThread = NULL;
    AdminListenThread *     pAdminListenThread = NULL;

public:
    void                    startThreads(bool isAdminOnly, bool isAdminEnabled);
    void                    killThreads();
};

#endif
