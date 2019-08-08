#include <stdint.h>

#include "logger.h"

#ifndef _INCL_FRAME
#define _INCL_FRAME

class Frame
{
private:
	bool			isAllocated;
	uint8_t	*		buffer;
	int				frameLength;

protected:
	uint8_t 		getMsgID();
	void			initialise(uint8_t * frame, int frameLength);

	Logger &		log = Logger::getInstance();

public:
	Frame();
	virtual ~Frame() {}

	bool		getIsAllocated();

	uint8_t *	getData();
	int			getDataLength();

	uint8_t *	getFrame();
	uint8_t		getFrameByteAt(int index);
	int			getFrameLength();

	uint8_t		getMessageID();
	uint8_t		getChecksum();
	uint8_t		getLength();

	virtual bool isRxFrame() = 0;
	virtual bool isTxFrame() = 0;
};

class TxFrame : public Frame
{
private:
	uint8_t *	frameBuffer;
	int			bufferLength;

public:
	TxFrame(uint8_t * frameBuffer, int bufferLength, uint8_t * data, int dataLength, uint8_t cmdCode);
	~TxFrame();

	uint8_t		getCmdCode();

	bool isRxFrame() {
		return false;
	}
	bool isTxFrame() {
		return true;
	}
};

class RxFrame : public Frame
{
public:
	RxFrame() : Frame() {}
	RxFrame(uint8_t * frame, int frameLength);

	uint8_t		getResponseCode();
	bool		isACK();
	bool		isNAK();
	uint8_t		getErrorCode();
	
	uint8_t	*	getData();
	int			getDataLength();

	bool		isChecksumValid();

	bool isRxFrame() {
		return true;
	}
	bool isTxFrame() {
		return false;
	}
};

#endif
