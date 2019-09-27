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

#define SERIAL_ENABLE_SELECT

static uint8_t emulated_cmd_buffer[MAX_REQUEST_MESSAGE_LENGTH];
static uint8_t emulated_rsp_buffer[MAX_RESPONSE_MESSAGE_LENGTH];

static int emulated_cmd_length = 0;
static int emulated_rsp_length = 0;

static CALIBRATION_DATA cd;

static void _build_response_frame(uint8_t * data, int dataLength)
{
	uint16_t			checksumTotal = 0;
	int					i;

	emulated_rsp_buffer[0] = MSG_CHAR_START;
	emulated_rsp_buffer[1] = (uint8_t)dataLength + 3;
	emulated_rsp_buffer[2] = emulated_cmd_buffer[2];
	emulated_rsp_buffer[3] = (emulated_cmd_buffer[3] << 4);
	emulated_rsp_buffer[4] = MSG_CHAR_ACK;
	
	checksumTotal = (emulated_rsp_buffer[2] + emulated_rsp_buffer[3]) + emulated_rsp_buffer[4];

	for (i = 0;i < dataLength;i++) {
		emulated_rsp_buffer[i + 5] = data[i];

		checksumTotal += emulated_rsp_buffer[i + 5];
	}

	emulated_rsp_buffer[5 + dataLength] = (uint8_t)(0x00FF - (checksumTotal & 0x00FF));
	emulated_rsp_buffer[6 + dataLength] = MSG_CHAR_END;

	emulated_rsp_length = dataLength + NUM_ACK_RSP_FRAME_BYTES;
}

SerialPort::SerialPort()
{
	isEmulationMode = false;
	expectedBytes = 0;

	memset(&cd, 0, sizeof(CALIBRATION_DATA));
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
	long	loopDelay = 50000L;
	long	timeout = 300000L;
	int		i = 0;

	bytesRead = read(fd, pBuffer, requestedBytes);

	log.logDebug("_receive_serial() - [%d] Read %d bytes, expecting %d", i, bytesRead, expectedBytes);

	/*
	** If we're expecting more bytes than we got
	** then wait for a bit and try to receive some more...
	*/
	while (bytesRead >= 0 && bytesRead < this->expectedBytes && loopTime < timeout) {
		usleep(loopDelay);

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

int SerialPort::_send_emulated(uint8_t * pBuffer, int writeLength)
{
	int						bytesWritten;
	uint8_t					cmdCode = 0;
	int						dataLength;
	uint8_t *				data;
	static const TPH		avgTph = { .temperature = 374, .pressure = 812, .humidity = 463 };
	static const TPH		maxTph = { .temperature = 392, .pressure = 874, .humidity = 562 };
	static const TPH		minTph = { .temperature = 332, .pressure = 799, .humidity = 401 };
	static const WINDSPEED	ws = { .avgWindspeed = 24, .maxWindspeed = 37 };
	static const RAINFALL	rf = { .avgRainfall = 11, .totalRainfall = 64 };
	static const char * 	schVer = "1.2.01 2019-07-30 17:37:20";
	static const char * 	avrVer = "1.2.009 [2019-09-14 17:37:20]";

	memset(emulated_cmd_buffer, 0, MAX_REQUEST_MESSAGE_LENGTH);
	memset(emulated_rsp_buffer, 0, MAX_RESPONSE_MESSAGE_LENGTH);

	memcpy(emulated_cmd_buffer, pBuffer, writeLength);

	emulated_cmd_length = writeLength;
	bytesWritten = writeLength;

	cmdCode = emulated_cmd_buffer[3];

	switch (cmdCode) {
		case RX_CMD_AVG_TPH:
			data = (uint8_t *)&avgTph;
			dataLength = sizeof(TPH);

			_build_response_frame(data, dataLength);
			break;

		case RX_CMD_MAX_TPH:
			data = (uint8_t *)&maxTph;
			dataLength = sizeof(TPH);

			_build_response_frame(data, dataLength);
			break;

		case RX_CMD_MIN_TPH:
			data = (uint8_t *)&minTph;
			dataLength = sizeof(TPH);

			_build_response_frame(data, dataLength);
			break;

		case RX_CMD_WINDSPEED:
			data = (uint8_t *)&ws;
			dataLength = sizeof(WINDSPEED);

			_build_response_frame(data, dataLength);
			break;

		case RX_CMD_RAINFALL:
			data = (uint8_t *)&rf;
			dataLength = sizeof(RAINFALL);

			_build_response_frame(data, dataLength);
			break;

		case RX_CMD_PING:
			data = (uint8_t *)NULL;
			dataLength = 0;

			_build_response_frame(data, dataLength);
			break;

		case RX_CMD_GET_AVR_VERSION:
			data = (uint8_t *)avrVer;
			dataLength = strlen(avrVer);

			_build_response_frame(data, dataLength);
			break;

		case RX_CMD_GET_SCHED_VERSION:
			data = (uint8_t *)schVer;
			dataLength = strlen(schVer);

			_build_response_frame(data, dataLength);
			break;

		case RX_CMD_GET_CALIBRATION_DATA:
			dataLength = sizeof(CALIBRATION_DATA);

			data = (uint8_t *)(&cd);

			_build_response_frame(data, dataLength);
			break;

		case RX_CMD_SET_CALIBRATION_DATA:
			memcpy(&cd, &emulated_cmd_buffer[4], emulated_cmd_buffer[1] - 2);

			data = (uint8_t *)NULL;
			dataLength = 0;

			_build_response_frame(data, dataLength);
			break;

		default:
			data = (uint8_t *)NULL;
			dataLength = 0;

			_build_response_frame(data, dataLength);
			break;
	}

	return bytesWritten;
}

int SerialPort::_receive_emulated(uint8_t * pBuffer, int requestedBytes)
{
	int		bytesRead = 0;
	
	usleep(100000L);

	/*
	** Block until we have some bytes to read...
	*/
	while (emulated_rsp_length == 0) {
		usleep(1000L);
	}

	bytesRead = emulated_rsp_length;
	//bytesRead = 10;

	memcpy(pBuffer, emulated_rsp_buffer, bytesRead);

	memset(emulated_cmd_buffer, 0, MAX_REQUEST_MESSAGE_LENGTH);
	memset(emulated_rsp_buffer, 0, MAX_RESPONSE_MESSAGE_LENGTH);

	emulated_cmd_length = 0;
	emulated_rsp_length = 0;

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
