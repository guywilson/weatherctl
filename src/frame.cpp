#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "exception.h"
#include "avrweather.h"
#include "frame.h"
#include "logger.h"

uint8_t				_msgID = 0;

Frame::Frame()
{
	this->isAllocated = true;
}

void Frame::initialise(uint8_t * frame, int frameLength)
{
	log.logDebug("Initialising with %d bytes", frameLength);

	this->buffer = frame;
	this->frameLength = frameLength;
}

uint8_t Frame::getMsgID()
{
	uint8_t	msgID;

	msgID = _msgID++;

	return msgID;
}

bool Frame::getIsAllocated()
{
	return isAllocated;
}

uint8_t * Frame::getData()
{
	if (this->getDataLength() == 0) {
		return NULL;
	}

	return &(this->buffer[4]);
}

int Frame::getDataLength()
{
	return this->frameLength - NUM_REQUEST_FRAME_BYTES;
}

uint8_t * Frame::getFrame()
{
	return this->buffer;
}

uint8_t Frame::getFrameByteAt(int index)
{
	if (index < 0 || index > this->frameLength - 1) {
		throw new Exception("Invalid index argument");
	}

	return this->buffer[index];
}

int Frame::getFrameLength()
{
	return this->frameLength;
}

uint8_t Frame::getLength()
{
	return this->getFrameByteAt(1);
}

uint8_t Frame::getMessageID()
{
	return this->getFrameByteAt(2);
}

uint8_t Frame::getChecksum()
{
	return this->getFrameByteAt(5 + this->getDataLength());
}

TxFrame::TxFrame(uint8_t * frameBuffer, int bufferLength, uint8_t * data, int dataLength, uint8_t cmdCode) : Frame()
{
	int				i;
	int				checksumTotal = 0;
	int				frameLength;

	this->frameBuffer = frameBuffer;
	this->bufferLength = bufferLength;

	frameLength = NUM_REQUEST_FRAME_BYTES + dataLength;

	if (bufferLength < (dataLength + NUM_REQUEST_FRAME_BYTES)) {
		throw new Exception("Frame buffer not large enough");
	}

	this->frameBuffer[0] = MSG_CHAR_START;
	this->frameBuffer[1] = 2 + dataLength;
	this->frameBuffer[2] = getMsgID();
	this->frameBuffer[3] = cmdCode;

	checksumTotal = (int)this->frameBuffer[2] + (int)this->frameBuffer[3];

	for (i = 0;i < dataLength;i++) {
		this->frameBuffer[i + 4] = data[i];
		checksumTotal += this->frameBuffer[i + 4];
	}

	this->frameBuffer[4 + dataLength] = (uint8_t)(0x00FF - (checksumTotal & 0x00FF));
	this->frameBuffer[5 + dataLength] = MSG_CHAR_END;

	initialise(this->frameBuffer, frameLength);
}

TxFrame::~TxFrame()
{
	memset(this->frameBuffer, 0, this->bufferLength);
}

uint8_t TxFrame::getCmdCode()
{
	return this->getFrameByteAt(3);
}

RxFrame::RxFrame(uint8_t * frame, int frameLength) : Frame()
{
	initialise(frame, frameLength);
}

uint8_t RxFrame::getResponseCode()
{
	return this->getFrameByteAt(3);
}

bool RxFrame::isACK()
{
	return (this->getFrameByteAt(4) == MSG_CHAR_ACK ? true : false);
}

bool RxFrame::isNAK()
{
	return (this->getFrameByteAt(4) == MSG_CHAR_NAK ? true : false);
}

bool RxFrame::isChecksumValid()
{
	int			i;
	uint16_t	checksumTotal = 0;

	for (i = 2;i < getFrameLength() - 1;i++) {
		checksumTotal += getFrame()[i];
	}

	return ((checksumTotal & 0x00FF) == 0x00FF ? true : false);
}

uint8_t RxFrame::getErrorCode()
{
	return this->getFrameByteAt(5);
}

uint8_t * RxFrame::getData()
{
	if (this->isACK()) {
		return &(this->getFrame()[5]);
	}
	else {
		return NULL;
	}
}

int RxFrame::getDataLength()
{
	if (this->isACK()) {
		return (this->getFrameByteAt(1) - 3);
	}
	else {
		return 0;
	}
}
