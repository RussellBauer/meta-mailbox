
#define SIGTERM_MSG "SIGTERM received.\n"
#define CREATE_SLAVE "echo slave-256reg 0x1060 > /sys/bus/i2c/devices/i2c-7/new_device"
#define KILL_SLAVE 	"echo 0x1060 > /sys/bus/i2c/devices/i2c-7/delete_device"
#define FILE_NAME "/sys/bus/i2c/devices/7-1060/slave-eeprom"

#define REGISTER_SIZE 128
#define REGISTER_COUNT 256
#define MYSLAVEADDRESS 0xc0		//make sure value matches the create (0x60 * 2), the leading 0x10 is a special notification to the driver
#define BCSLAVEADDRESS 0x70
#define MAILBOXDATASIZE REGISTER_SIZE

#define VERSION         (0*REGISTER_SIZE)
#define BMC_STATUS      (1*REGISTER_SIZE)
#define BC_STATUS       (2*REGISTER_SIZE)
#define BMC_HB          (3*REGISTER_SIZE)
#define BC_HB           (4*REGISTER_SIZE)
#define REQ_MAILBOX     (5*REGISTER_SIZE)
#define ACK_MAILBOX     (6*REGISTER_SIZE)
#define REQ_DATA        (7*REGISTER_SIZE)
#define ACK_DATA        (8*REGISTER_SIZE)
#define FAST_DATA_BC    (9*REGISTER_SIZE)
#define FAST_DATA_BMC   (10*REGISTER_SIZE)

#pragma pack(1)

union REQ_PACKET{

	unsigned char buffer[131];
	struct{
		unsigned char lastMailBox;
		unsigned char reqDataPktSize;
		unsigned char BMCi2cAddress;
		unsigned char netFunc_LUN;
		unsigned char headerCheckSum;
		unsigned char BCi2cAddress;
		unsigned char sequence;
		unsigned char command;
		unsigned char payLoad[123];
	}reqPacket;
	struct{
		unsigned char lastMailBox;
		unsigned char reqDataPktSize;
		unsigned char BCi2cAddress;
		unsigned char netFunc_LUN;
		unsigned char headerCheckSum;
		unsigned char BMCi2cAddress;
		unsigned char sequence;
		unsigned char command;
		unsigned char completionCode;
		unsigned char payLoad[122];
	}ackPacket;

};

union REQ_PACKET reqBuffer;
union REQ_PACKET ackBuffer;

#define OVERHEAD 4
#define PKTOVERHEAD 7

#define IPMI_CMD_GET_SYSTEM_INFO  			0x1859
#define IPMI_CMD_GET_SYSTEM_GUID    		0x1837
#define SC_BMC_SET_CHASSIS_CONF				0xc011
#define SC_BMC_SET_CHASSIS_NAME				0xc013
#define SC_BMC_GET_CHASSIS_NAME     		0xc014
#define IPMI_CMD_SYNC_CHASSIS_ST			0xc026
#define SC_BMC_SET_SECONDARY_PSU_INFO  		0xc04c
#define SC_BMC_GET_INTERNAL_VARIABLE_CMD 	0xc027
#define IPMI_OEM_CMD_iDRAC_POST_CODE		0xc0d4
#define SC_BMC_SET_CHASSIS_POWER_READINGS	0xc02f
#define IPMI_CMD_GET_DEVICE_ID			 	0x1801
#define SC_BMC_GET_PWM 						0xc08c
#define IPMI_DCMI_CMD_GET_POWER_READING 	0xb002
#define IPMI_CMD_GET_LAN_CONFIG_PARA        0x3002
#define IP_ADDRESS							0x03
#define MAC_ADDRESS							0x05
#define BMCSubCommand						0x30c8
#define SetPSUDataForIBMGreyJoy				0x13
#define SC_BMC_GET_PROTOCOL_VERSION			0xC02C
#define SC_BMC_SET_SENSOR_INFO				0xC015 
#define SC_BMC_DCS_OEM_WRAPPER_CMDxSC_BMC_SET_THERMAL_PROPERTIES	0xc0c8

