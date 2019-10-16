#ifndef _INCL_AVRWEATHER
#define _INCL_AVRWEATHER

#include "frame.h"

#define LOG_RXTX

/*
** Request frame (6 - 80 bytes)
** <START><LENGTH><MSG_ID><CMD><DATA (0 - 74)><CHECKSUM><END>
**                 |                        |
**                  --included in checksum--
**
** ACK response frame
** <START><LENGTH><MSG_ID><RSP><ACK+DATA  (0 - 73)><CHECKSUM><END>
**
** NAK response frame
** <START><LENGTH><MSG_ID><RSP><NAK+ERR_CODE (2 bytes)><CHECKSUM><END>
**
**
** The checksum is calculated as follows:
**
** The sender adds all the data bytes plus the cmd and msg id, taking
** just the least significant byte and subtracting that from 0xFF gives
** the checksum.
**
** The receiver adds all the data bytes plus the cmd, msg id and checksum,
** the least significant byte should equal 0xFF if the checksum is valid.
*/

#define NUM_REQUEST_FRAME_BYTES		  6
#define MAX_DATA_LENGTH				 74
#define MAX_REQUEST_MESSAGE_LENGTH	(NUM_REQUEST_FRAME_BYTES + MAX_DATA_LENGTH)

#define NUM_ACK_RSP_FRAME_BYTES		  7
#define NUM_NAK_RSP_FRAME_BYTES		  8
#define MAX_RESPONSE_MESSAGE_LENGTH	 MAX_REQUEST_MESSAGE_LENGTH

#define MAX_CMD_FRAME_LENGTH		 MAX_DATA_LENGTH + 2		// Data + msgID + cmd

#define MSG_CHAR_START				0x7E
#define MSG_CHAR_END				0x81

#define MSG_CHAR_ACK				0x06
#define MSG_CHAR_NAK				0x15

#define MSG_NAK_UNKNOWN_CMD			0x01
#define MSG_NAK_INVALID_CHECKSUM	0x02
#define MSG_NAK_DATA_ERROR			0x04
#define MSG_NAK_DATA_OVERRUN		0x08
#define MSG_NAK_NO_END_CHAR			0x10

#define RX_STATE_START				0x01
#define RX_STATE_LENGTH				0x02
#define RX_STATE_MSGID				0x03
#define RX_STATE_CMD				0x04
#define RX_STATE_DATA				0x05
#define RX_STATE_CHECKSUM			0x06
#define RX_STATE_END				0x07

#define RX_STATE_RESPONSE			0x10
#define RX_STATE_RESPTYPE			0x11
#define RX_STATE_ACK				0x12
#define RX_STATE_NAK				0x13
#define RX_STATE_ERRCODE			0x14

#define RX_CMD_PING					0x0F
#define RX_CMD_TPH					0x01
#define RX_CMD_RESET_MINMAX			0x02
#define RX_CMD_WINDSPEED			0x03
#define RX_CMD_RAINFALL				0x04
#define RX_CMD_CPU_PERCENTAGE		0x05
#define RX_CMD_WDT_DISABLE			0x06
#define RX_CMD_GET_DASHBOARD		0x07

#define RX_RSP_PING					(RX_CMD_PING << 4)
#define RX_RSP_TPH					(RX_CMD_TPH << 4)
#define RX_RSP_RESET_MINMAX			(RX_CMD_RESET_MINMAX << 4)
#define RX_RSP_WINDSPEED			(RX_CMD_WINDSPEED << 4)
#define RX_RSP_RAINFALL				(RX_CMD_RAINFALL << 4)
#define RX_RSP_CPU_PERCENTAGE		(RX_CMD_CPU_PERCENTAGE << 4)
#define RX_RSP_WDT_DISABLE			(RX_CMD_WDT_DISABLE << 4)
#define RX_RSP_GET_DASHBOARD		(RX_CMD_GET_DASHBOARD << 4)

#define AVR_RESET_PIN				12

#define PI								(double)(3.14159265)

/*
** The diameter of the anemometer blades in metres...
*/
#define BLADE_DIAMETER					(double)(0.1)
#define RPS_TO_KPH_SCALE_FACTOR			((PI * BLADE_DIAMETER * (double)3600) / (double)1000)

#define TIPPING_BUCKET_VOLUME			(double)(1750)		    // In mm^3 (1 cm^3 | 1 ml = 1000mm^3)
#define COLLECTING_FUNNEL_RADIUS		(double)(50)			// In mm

#define TIPS_TO_MM_SCALE_FACTOR			(TIPPING_BUCKET_VOLUME / (PI * COLLECTING_FUNNEL_RADIUS * COLLECTING_FUNNEL_RADIUS))

typedef struct {
    uint16_t           temperature;
    uint16_t           pressure;
    uint16_t           humidity;
}
TPH;

typedef struct {
    TPH         min;
    TPH         max;
    TPH         current;
}
TPH_PACKET;

typedef struct {
    uint16_t           avgWindspeed;
    uint16_t           maxWindspeed;
}
WINDSPEED;

typedef struct {
    uint16_t           avgRainfall;
    uint16_t           totalRainfall;
}
RAINFALL;

typedef struct {
    uint32_t        uptime;
    uint32_t        numMessagesProcessed;
    uint32_t        numTasksRun;

    char            szAVRVersion[30];
    char            szSchedVersion[30];
}
DASHBOARD;

typedef struct
{
	uint32_t		idleCount;
	uint32_t		busyCount;
}
CPU_RATIO;

void 		resetAVR();
RxFrame * 	send_receive(TxFrame * pTxFrame);
void 		fire_forget(TxFrame * pTxFrame);
void		printFrame(uint8_t * buffer, int bufferLength);
void		processResponse(uint8_t * response, int responseLength);
double      getActualTemperature(uint16_t sensorValue);
double      getActualPressure(uint16_t sensorValue);
double      getActualHumidity(uint16_t sensorValue);
double      getActualWindspeed(uint16_t sensorValue);
double      getActualRainfall(uint16_t sensorValue);

#endif
