#include <iostream>
#include <queue>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include "avrweather.h"
#include "exception.h"
#include "queuemgr.h"

using namespace std;

QueueMgr::QueueMgr()
{
	if (pthread_mutex_init(&txLock, NULL)) {
        throw new Exception("Failed to create TX mutex");
	}

	if (pthread_mutex_init(&rxLock, NULL)) {
        throw new Exception("Failed to create RX mutex");
	}
}

QueueMgr::~QueueMgr()
{
	pthread_mutex_destroy(&txLock);
	pthread_mutex_destroy(&rxLock);
}

TxFrame * QueueMgr::peekTx()
{
    TxFrame *     frame = NULL;

	pthread_mutex_lock(&txLock);
    
    if (!txQueue.empty()) {
        frame = txQueue.front();
    }
	
    pthread_mutex_unlock(&txLock);

    return frame;
}

TxFrame * QueueMgr::popTx()
{
    TxFrame *     frame = NULL;

	pthread_mutex_lock(&txLock);
    
    if (!txQueue.empty()) {
        frame = txQueue.front();
        txQueue.pop();
    }
	
    pthread_mutex_unlock(&txLock);

    return frame;
}

void QueueMgr::pushTx(TxFrame * pFrame)
{
	pthread_mutex_lock(&txLock);
    txQueue.push(pFrame);
    pthread_mutex_unlock(&txLock);
}

bool QueueMgr::isTxQueueEmpty()
{
    bool    isEmpty;

	pthread_mutex_lock(&txLock);
    isEmpty = txQueue.empty();
    pthread_mutex_unlock(&txLock);

    return isEmpty;
}

RxFrame * QueueMgr::peekRx()
{
    RxFrame *     frame = NULL;

	pthread_mutex_lock(&rxLock);
    
    if (!rxQueue.empty()) {
        frame = rxQueue.front();
    }
	
    pthread_mutex_unlock(&rxLock);

    return frame;
}

RxFrame * QueueMgr::popRx()
{
    RxFrame *     frame = NULL;

	pthread_mutex_lock(&rxLock);
    
    if (!rxQueue.empty()) {
        frame = rxQueue.front();
        rxQueue.pop();
    }
	
    pthread_mutex_unlock(&rxLock);

    return frame;
}

void QueueMgr::pushRx(RxFrame * pFrame)
{
	pthread_mutex_lock(&rxLock);
    rxQueue.push(pFrame);
    pthread_mutex_unlock(&rxLock);
}

bool QueueMgr::isRxQueueEmpty()
{
    bool    isEmpty;

	pthread_mutex_lock(&rxLock);
    isEmpty = rxQueue.empty();
    pthread_mutex_unlock(&rxLock);

    return isEmpty;
}

PostData * QueueMgr::peekWebPost()
{
    PostData *     pPostData = NULL;

	pthread_mutex_lock(&webPostLock);
    
    if (!webPostQueue.empty()) {
        pPostData = webPostQueue.front();
    }
	
    pthread_mutex_unlock(&webPostLock);

    return pPostData;
}

PostData * QueueMgr::popWebPost()
{
    PostData *     pPostData = NULL;

	pthread_mutex_lock(&webPostLock);
    
    if (!webPostQueue.empty()) {
        pPostData = webPostQueue.front();
        webPostQueue.pop();
    }
	
    pthread_mutex_unlock(&webPostLock);

    return pPostData;
}

void QueueMgr::pushWebPost(PostData * pPostData)
{
	pthread_mutex_lock(&webPostLock);
    webPostQueue.push(pPostData);
    pthread_mutex_unlock(&webPostLock);
}

bool QueueMgr::isWebPostQueueEmpty()
{
    bool    isEmpty;

	pthread_mutex_lock(&webPostLock);
    isEmpty = webPostQueue.empty();
    pthread_mutex_unlock(&webPostLock);

    return isEmpty;
}
