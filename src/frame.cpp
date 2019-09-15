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
	clear();
	this->frameLength = 0;
}

Frame::~Frame()
{
	clear();
}

void Frame::initialise(int frameLength)
{
	this->frameLength = frameLength;
}

void Frame::initialise(uint8_t * frame, int frameLength)
{
	memcpy(this->buffer, frame, frameLength);
	initialise(frameLength);
}

void Frame::clear()
{
	memset(this->buffer, 0, MAX_REQUEST_MESSAGE_LENGTH);
}

uint8_t Frame::getMsgID()
{
	uint8_t	msgID;

	msgID = _msgID++;

	return msgID;
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
	int dataLength = 0;

	if (this->frameLength == 0) {
		dataLength = 0;
	}
	else {
		dataLength = this->frameLength - NUM_REQUEST_FRAME_BYTES;
	}

	return dataLength;
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

TxFrame::TxFrame(uint8_t * data, int dataLength, uint8_t cmdCode) : Frame()
{
	int				i;
	int				checksumTotal = 0;
	int				frameLength;
	uint8_t *		buffer;

	buffer = getFrame();

	frameLength = NUM_REQUEST_FRAME_BYTES + dataLength;

	buffer[0] = MSG_CHAR_START;
	buffer[1] = 2 + dataLength;
	buffer[2] = getMsgID();
	buffer[3] = cmdCode;

	checksumTotal = (int)buffer[2] + (int)buffer[3];

	for (i = 0;i < dataLength;i++) {
		buffer[i + 4] = data[i];
		checksumTotal += buffer[i + 4];
	}

	buffer[4 + dataLength] = (uint8_t)(0x00FF - (checksumTotal & 0x00FF));
	buffer[5 + dataLength] = MSG_CHAR_END;

	initialise(frameLength);
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
