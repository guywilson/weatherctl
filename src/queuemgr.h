#include <queue>
#include <stdint.h>

#include "frame.h"
#include "postdata.h"

using namespace std;

#ifndef _INCL_QUEUEMGR
#define _INCL_QUEUEMGR

class QueueMgr
{
public:
    static QueueMgr & getInstance() {
        static QueueMgr instance;
        return instance;
    }

private:
    QueueMgr();

    queue<TxFrame *>    txQueue;
    queue<RxFrame *>    rxQueue;
    queue<PostData *>   webPostQueue;

    pthread_mutex_t 	txLock;
    pthread_mutex_t 	rxLock;
    pthread_mutex_t 	webPostLock;

public:
    ~QueueMgr();
    
    TxFrame *           peekTx();
    TxFrame *           popTx();
    void                pushTx(TxFrame * pFrame);
    bool                isTxQueueEmpty();

    RxFrame *           peekRx();
    RxFrame *           popRx();
    void                pushRx(RxFrame * pFrame);
    bool                isRxQueueEmpty();

    PostData *          peekWebPost();
    PostData *          popWebPost();
    void                pushWebPost(PostData * pszPost);
    bool                isWebPostQueueEmpty();
};

#endif