#ifndef _INCL_RXTXTHREAD
#define _INCL_RXTXTHREAD

int         startThreads(bool isAdminOnly);
void        killThreads();
void *      txCmdThread(void * pArgs);
void *      versionPostThread(void * pArgs);
void *      webPostThread(void * pArgs);
void *      webListenerThread(void * pArgs);

#endif