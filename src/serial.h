#include <termios.h>

#include "logger.h"

#ifndef _INCL_SERIAL
#define _INCL_SERIAL

class SerialPort
{
public:
    static SerialPort & getInstance() {
        static SerialPort instance;
        return instance;
    }

private:
	int					fd;

	int					expectedBytes;
	struct termios		new_settings;
	struct termios		old_settings;

	bool				isEmulationMode;

	Logger & 			log = Logger::getInstance();
	
	SerialPort();

	void 				_openSerialPort(char * pszPort, int baudRate, bool isBlocking);

	int					_send_serial(uint8_t * pBuffer, int writeLength);
	int					_receive_serial(uint8_t * pBuffer, int requestedBytes);

	int					_send_emulated(uint8_t * pBuffer, int writeLength);
	int					_receive_emulated(uint8_t * pBuffer, int requestedBytes);

public:
						~SerialPort();

	void 				openPort(char * pszPort, int baudRate, bool isBlocking);
	void				closePort();

	void				setEmulationMode();

	void				setExpectedBytes(int size);

	static int 			mapBaudRate(int speed);

	int					send(uint8_t * pBuffer, int writeLength);
	int					receive(uint8_t * pBuffer, int requestedBytes);
};

#endif
