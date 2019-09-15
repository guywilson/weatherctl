#include <stdint.h>

#ifndef _INCL_FRAME
#define _INCL_FRAME

#include "logger.h"

class Frame
{
private:
	uint8_t			buffer[80];
	int				frameLength;

protected:
	uint8_t 		getMsgID();
	void			initialise(int frameLength);
	void			initialise(uint8_t * frame, int frameLength);
	void			clear();

	Logger &		log = Logger::getInstance();

public:
	Frame();
	virtual ~Frame();

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
public:
	TxFrame(uint8_t * data, int dataLength, uint8_t cmdCode);
	virtual ~TxFrame() {}

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
	virtual ~RxFrame() {}

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
