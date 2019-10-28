#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include "serial.h"
#include "exception.h"
#include "avrweather.h"
#include "logger.h"
#include "frame.h"
#include "posixthread.h"

using namespace std;

#define SERIAL_ENABLE_SELECT

queue<serial_frame *>    _emulatedTxQueue;
queue<serial_frame *>    _emulatedRxQueue;

pthread_t			tidAVREmulator;

static serial_frame * _build_response_frame(serial_frame * rxFrame, uint8_t * data, int dataLength)
{
	uint16_t			checksumTotal = 0;
	int					i;

	serial_frame * txFrame = (serial_frame *)malloc(sizeof(serial_frame));
	
	txFrame->frame[0] = MSG_CHAR_START;
	txFrame->frame[1] = dataLength + 3;
	txFrame->frame[2] = rxFrame->frame[2];
	txFrame->frame[3] = (rxFrame->frame[3] << 4);
	txFrame->frame[4] = MSG_CHAR_ACK;

	checksumTotal = (txFrame->frame[2] + txFrame->frame[3]) + txFrame->frame[4];

	for (i = 0;i < dataLength;i++) {
		txFrame->frame[i + 5] = data[i];
		checksumTotal += txFrame->frame[i + 5];
	}

	txFrame->frame[5 + dataLength] = (uint8_t)(0x00FF - (checksumTotal & 0x00FF));
	txFrame->frame[6 + dataLength] = MSG_CHAR_END;

	txFrame->framelength = dataLength + NUM_ACK_RSP_FRAME_BYTES;

	return txFrame;
}

void * avrEmulator(void * pArgs)
{
	uint8_t					cmdCode;
	int						dataLength;
	uint8_t 				data[80];
	DASHBOARD				db;
	CPU_RATIO				cpu;
	TPH_PACKET				tph;
	static const TPH		avgTph = { .temperature = 374, .pressure = 812, .humidity = 463 };
	static const TPH		maxTph = { .temperature = 392, .pressure = 874, .humidity = 562 };
	static const TPH		minTph = { .temperature = 332, .pressure = 799, .humidity = 401 };
	static const WINDSPEED	ws = { .avgWindspeed = 24, .maxWindspeed = 37 };
	static const RAINFALL	rf = { .avgRainfall = 11, .totalRainfall = 64 };
	static const char * 	schVer = "1.2.001 [2019-07-30 17:37:20]";
	static const char * 	avrVer = "1.4.001 [2019-09-27 08:13:53]";

	strcpy(db.szAVRVersion, avrVer);
	strcpy(db.szSchedVersion, schVer);

	db.uptime = 0;
	db.numMessagesProcessed = 0;
	db.numTasksRun = 0;

	cpu.idleCount = 0;
	cpu.busyCount = 0;

	while (1) {
		cpu.idleCount++;

		if (!_emulatedTxQueue.empty()) {
			serial_frame * rxFrame = _emulatedTxQueue.front();
			_emulatedTxQueue.pop();

			PosixThread::sleep(1L);

			cmdCode = rxFrame->frame[3];

			serial_frame * txFrame;

			cpu.busyCount++;

			switch (cmdCode) {
				case RX_CMD_TPH:
					memcpy(&tph.current, &avgTph, sizeof(TPH));
					memcpy(&tph.max, &maxTph, sizeof(TPH));
					memcpy(&tph.min, &minTph, sizeof(TPH));
					memcpy(data, &tph, sizeof(TPH_PACKET));
					dataLength = sizeof(TPH_PACKET);

					txFrame = _build_response_frame(rxFrame, data, dataLength);
					break;

				case RX_CMD_WINDSPEED:
					memcpy(data, &ws, sizeof(WINDSPEED));
					dataLength = sizeof(WINDSPEED);

					txFrame = _build_response_frame(rxFrame, data, dataLength);
					break;

				case RX_CMD_RAINFALL:
					memcpy(data, &rf, sizeof(RAINFALL));
					dataLength = sizeof(RAINFALL);

					txFrame = _build_response_frame(rxFrame, data, dataLength);
					break;

				case RX_CMD_PING:
					txFrame = _build_response_frame(rxFrame, NULL, 0);
					break;

				case RX_CMD_CPU_PERCENTAGE:
					cpu.idleCount = cpu.idleCount - cpu.busyCount;

					memcpy(data, &cpu, sizeof(CPU_RATIO));
					dataLength = sizeof(CPU_RATIO);

					txFrame = _build_response_frame(rxFrame, data, dataLength);
					break;

				case RX_CMD_GET_DASHBOARD:
					memcpy(data, &db, sizeof(DASHBOARD));
					dataLength = sizeof(DASHBOARD);

					txFrame = _build_response_frame(rxFrame, data, dataLength);
					break;

				default:
					txFrame = _build_response_frame(rxFrame, NULL, 0);
					break;
			}

			db.numMessagesProcessed++;

			free(rxFrame);

			_emulatedRxQueue.push(txFrame);
		}

		db.uptime += 10;
		db.numTasksRun += 7;

		PosixThread::sleep(1L);
	}
}

int SerialPort::_send_emulated(uint8_t * pBuffer, int writeLength)
{
	int						bytesWritten;
	
	serial_frame *	frame = (serial_frame *)malloc(sizeof(serial_frame));

	memcpy(frame->frame, pBuffer, writeLength);
	frame->framelength = writeLength;

	_emulatedTxQueue.push(frame);

	bytesWritten = writeLength;

	return bytesWritten;
}

int SerialPort::_receive_emulated(uint8_t * pBuffer, int requestedBytes)
{
	int		bytesRead = 0;
	
	/*
	** Block until we have some bytes to read...
	*/
	while (_emulatedRxQueue.empty()) {
		PosixThread::sleep(10L);
	}

	serial_frame * frame = _emulatedRxQueue.front();
	_emulatedRxQueue.pop();

	bytesRead = frame->framelength;

	memcpy(pBuffer, frame->frame, frame->framelength);

	free(frame);

	return bytesRead;
}

SerialPort::SerialPort()
{
	isEmulationMode = false;
	expectedBytes = 0;
}

SerialPort::~SerialPort()
{
	/*
	 * Set the old port parameters...
	 */
	if ((tcsetattr(fd, TCSANOW, &old_settings)) != 0) {
        log.logError("Error setting old attributes");
	}

	closePort();
}

void SerialPort::_openSerialPort(const char * pszPort, int baudRate, bool isBlocking)
{
	char				szExceptionText[1024];
	int					flags;

	flags = O_RDWR | O_NOCTTY | O_NDELAY;

	if (!isBlocking) {
		flags |= O_NONBLOCK;
	}

	/*
	 * Open the serial port...
	 */
	fd = open(pszPort, flags);

	if (fd < 0) {
		sprintf(szExceptionText, "Error opening %s: %s", pszPort, strerror(errno));
        throw new Exception(szExceptionText);
    }

	/*
	 * Get current port parameters...
	 */
	tcgetattr(fd, &new_settings);

	old_settings = new_settings;

	new_settings.c_iflag = IGNBRK;
	new_settings.c_oflag = 0;
	new_settings.c_lflag = 0;
	new_settings.c_cflag = (CS8 | CREAD | CLOCAL);

	new_settings.c_cc[VMIN]  = NUM_ACK_RSP_FRAME_BYTES;		// Read minimum 7 characters
	new_settings.c_cc[VTIME] = 3;  							// wait...

	/*
	 * Set the baud rate...
	 */
	cfsetispeed(&new_settings, baudRate);
	cfsetospeed(&new_settings, baudRate);

	/*
	 * Set the new port parameters...
	 */
	if ((tcsetattr(fd, TCSANOW, &new_settings)) != 0) {
		close(fd);
        throw new Exception("Error setting attributes");
	}

	if (isBlocking) {
		int rc;
		rc = fcntl(fd, F_GETFL, 0);

		if (rc != -1) {
			fcntl(fd, F_SETFL, rc & ~O_NONBLOCK);
		}
	}
}

void SerialPort::openPort(const char * pszPort, int baudRate, bool isBlocking, bool isEmulation)
{
	this->isEmulationMode = isEmulation;

	if (!isEmulationMode) {
		_openSerialPort(pszPort, baudRate, isBlocking);
	}
	else {
		Logger & log = Logger::getInstance();

		int err;

		err = pthread_create(&tidAVREmulator, NULL, &avrEmulator, NULL);

		if (err != 0) {
			log.logError("ERROR! Can't create avrEmulator() thread :[%s]", strerror(err));
			throw new Exception("Cannot create emulator thread");
		}
		else {
			log.logInfo("Thread avrEmulator() created successfully");
		}
	}
}

void SerialPort::closePort()
{
	close(fd);
}

void SerialPort::setExpectedBytes(int size)
{
	this->expectedBytes = size;
}

int SerialPort::_send_serial(uint8_t * pBuffer, int writeLength)
{
	int		bytesWritten;

	bytesWritten = write(fd, pBuffer, writeLength);

	if (bytesWritten < 0) {
		throw new Exception("Error writing to port");
	}

	return bytesWritten;
}

int SerialPort::_receive_serial(uint8_t * pBuffer, int requestedBytes)
{
	int		bytesRead = 0;
	long	loopTime = 0L;
	long	loopDelay = 50L;
	long	timeout = 300L;
	int		i = 0;

	bytesRead = read(fd, pBuffer, requestedBytes);

	log.logDebug("_receive_serial() - [%d] Read %d bytes, expecting %d", i, bytesRead, expectedBytes);

	/*
	** If we're expecting more bytes than we got
	** then wait for a bit and try to receive some more...
	*/
	while (bytesRead >= 0 && bytesRead < this->expectedBytes && loopTime < timeout) {
		PosixThread::sleep(loopDelay);

		loopTime += loopDelay;

		bytesRead += read(fd, &pBuffer[bytesRead], (requestedBytes - bytesRead));

		i++;

		log.logDebug("_receive_serial() - [%d] Read %d bytes", i, bytesRead);
	}

	if (bytesRead < 0) {
		char szErrorString[80];
		sprintf(szErrorString, "Error receiving on serial port [%s]", strerror(errno));
		throw new Exception(szErrorString);
	}

	/*
	** Reset expected bytes...
	*/
	this->expectedBytes = 0;

	return bytesRead;
}

int SerialPort::send(uint8_t * pBuffer, int writeLength)
{
	if (isEmulationMode) {
		return _send_emulated(pBuffer, writeLength);
	}
	else {
		return _send_serial(pBuffer, writeLength);
	}
}

int SerialPort::receive(uint8_t * pBuffer, int requestedBytes)
{
	if (isEmulationMode) {
		return _receive_emulated(pBuffer, requestedBytes);
	}
	else {
		return _receive_serial(pBuffer, requestedBytes);
	}
}

int SerialPort::mapBaudRate(int baud)
{
	int			baudConst = B9600;
	Logger & 	log = Logger::getInstance();

	switch (baud) {
		case 50:
			baudConst = B50;
			break;

		case 75:
			baudConst = B75;
			break;

		case 110:
			baudConst = B110;
			break;

		case 134:
			baudConst = B134;
			break;

		case 150:
			baudConst = B150;
			break;

		case 200:
			baudConst = B200;
			break;

		case 300:
			baudConst = B300;
			break;

		case 600:
			baudConst = B600;
			break;

		case 1200:
			baudConst = B1200;
			break;

		case 1800:
			baudConst = B1800;
			break;

		case 2400:
			baudConst = B2400;
			break;

		case 4800:
			baudConst = B4800;
			break;

		case 9600:
			baudConst = B9600;
			break;

		case 19200:
			baudConst = B19200;
			break;

		case 38400:
			baudConst = B38400;
			break;

		case 57600:
			baudConst = B57600;
			break;

		case 115200:
			baudConst = B115200;
			break;

		case 230400:
			baudConst = B230400;
			break;

		default:
			log.logError("Unsupported baud rate [%d], supported rates are 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400", baud);
			log.logInfo("Defaulting to 9600 baud");
			break;
	}

	return baudConst;
}
