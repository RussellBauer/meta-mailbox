//should build
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>
#include <systemd/sd-bus.h>

//for network stuff
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>

//this is for adding the ioctl control and user signals
#include <sys/ioctl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define IOC_MAGIC 'Q' // defines the magic number
#define IOCTL_SC_BMC_COMM_REG _IO(IOC_MAGIC,0) // defines our ioctl call
#define IOCTL_SC_BMC_COMM_UNREG _IO(IOC_MAGIC,1) // defines our ioctl call

#include "catch.h"

int processFailure(char);

//here's the plan.... part 2
//register for a signal from the driver
//sleep ... teh signal will cut teh sleep short
//on check mailbox is req is active, do the deed, 
//open up the file and look to see if we have a mailbox request (REQ_MAILBOX)
//if a request is present (!0) change from last reading
//read the mailbox (REQ_DATA)
//send info to the header validation code (our slave address and first 2 bytes zero byte sum)
//if that is good, send the command data to the NetFun Handler
//        Net function handler will call the assocoated command handler
//        Fill out responce (ACK_DATA) based on command completion( either Good (0x00) with data, or !0 for error)
//Set the ACK_MAILBOX to the same value from the REQ_MAILBOX
//read REQ_MAILBOX for 0 to indicate BC has read responce
//write ACK_MAILBOX to 0 to complete the command sequence
//rense and repeat




//commands to do
//commandName["302F"] = "CS-OEM    SC_BMC_SET_CHASSIS_POWER_READINGS                       "
//Write -> iDrac => Seq: 0x78 NetFn/CMD CS-OEM    SC_BMC_SET_CHASSIS_POWER_READINGS                        :[C0 20 70 78 2F 03 AB 02 55 00 02 FF FF FF E5]
//Write -> MC    => Seq: 0x78 NetFn/CMD cs-oem    SC_BMC_SET_CHASSIS_POWER_READINGS                        :[C4 CC 20 78 2F 00 39]










void sig_term_handler(int signum, siginfo_t *info, void *ptr)
{
    write(STDERR_FILENO, SIGTERM_MSG, sizeof(SIGTERM_MSG));
    system(KILL_SLAVE);
    exit(0);	
}

void catch_sigterm()
{
    static struct sigaction _sigact;

    memset(&_sigact, 0, sizeof(_sigact));
    _sigact.sa_sigaction = sig_term_handler;
    _sigact.sa_flags = SA_SIGINFO;

    sigaction(SIGTERM, &_sigact, NULL);
}

//add new signal catch
static void sig_usr(int); /* one handler for both signals */
int sig1active = 0;
static void
sig_usr(int signo)      /* argument is signal number */
{
    if (signo == SIGUSR1){
        printf("received SIGUSR1\n");
		sig1active = 1;
	}
    else if (signo == SIGUSR2)
        printf("received SIGUSR2\n");
    else
        fprintf(stderr, "received signal %d\n", signo);
//    printf("got a signal...\n");
}



unsigned char checkSumData(char *buffer, int length){
int x;
int checkSum = 0;

	for(x = 0; x < length;x++)
		checkSum += buffer[x];

	return (0x100 - (checkSum & 0xff));
}

//checks to see if a new mailbox value is set (new comamnd or complete sequence
//if there is a non-zero mailbox copy the data (with slave address prependd) and set the lenght
int checkMailBox(){

FILE *fptr;
char mailBox;
int returnValue = 0;

	 if(!sig1active)
		return returnValue;
	 else
		sig1active = 0;

	 reqBuffer.reqPacket.reqDataPktSize = 0;	
	 fptr = fopen(FILE_NAME,"rb");
	 fseek(fptr, REQ_MAILBOX, SEEK_SET);
#if 1
	 fread(&mailBox, 1, 1, fptr);
     fread(&reqBuffer.reqPacket.reqDataPktSize, 1, 1, fptr);
	 if( reqBuffer.reqPacket.reqDataPktSize == 0 && mailBox != 0){
		printf("skip\n");
		goto waitOneMoreCycle;
	 }
#else
	 mailBox = fgetc( fptr );
	 reqBuffer.reqPacket.reqDataPktSize = fgetc( fptr );
#endif
	 if(mailBox != reqBuffer.reqPacket.lastMailBox){
		reqBuffer.reqPacket.lastMailBox = mailBox;
		if(mailBox != 0x00){
	 		fseek(fptr, REQ_DATA, SEEK_SET);
			reqBuffer.reqPacket.BMCi2cAddress = MYSLAVEADDRESS;
			reqBuffer.reqPacket.BCi2cAddress = BCSLAVEADDRESS;
#if 1
			fread(&reqBuffer.reqPacket.netFunc_LUN, reqBuffer.reqPacket.reqDataPktSize,1 , fptr);
#else
			fgets(&reqBuffer.reqPacket.netFunc_LUN, MAILBOXDATASIZE , fptr);
#endif
	 		reqBuffer.reqPacket.reqDataPktSize++;	//+1 for adding in my slave address

		}else{
			reqBuffer.reqPacket.reqDataPktSize = 0;
		}
		returnValue = 1;
	 }else{
		returnValue = 0;
	 }

waitOneMoreCycle:
	fclose(fptr);
	return returnValue;
}


//this will check the header and payload area
//0- OK
//1- header failed
//2-payload failed
int validateComamndData(char *buffer, int length){
int x;
int chksum = 0;

	for(x = 0;x<3;x++){
		chksum += buffer[x];
	}
   	if(chksum & 0xff)
   		return 1;
	for(;x<length;x++){				//take up where we left off
		chksum += buffer[x];
	}
   	if(chksum & 0xff){
		//printf("\nCheckSum %04x\n",chksum);
   		return 2;
	}

   return 0;

																					   
}

//commandName["302F"] = "CS-OEM    SC_BMC_SET_CHASSIS_POWER_READINGS                       "
//Write -> iDrac => Seq: 0x78 NetFn/CMD CS-OEM    SC_BMC_SET_CHASSIS_POWER_READINGS                        :[C0 20 70 78 2F 03 AB 02 55 00 02 FF FF FF E5]
//Write -> MC    => Seq: 0x78 NetFn/CMD cs-oem    SC_BMC_SET_CHASSIS_POWER_READINGS                        :[C4 CC 20 78 2F 00 39]
int processSC_BMC_SET_CHASSIS_POWER_READINGS (){

	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00; 	
	ackBuffer.ackPacket.payLoad[0] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 4);	 

	ackBuffer.ackPacket.reqDataPktSize = 7;


return 0;
}



//commandName["3015"] = "CS-OEM    SC_BMC_SET_SENSOR_INFO                                  "
//Write -> iDrac => Seq: 0x6C NetFn/CMD CS-OEM    SC_BMC_SET_SENSOR_INFO                                   :[C0 20 70 6C 15 20 FE 2A 00 17 00 00 00 00 00 07 00 EC 00 E4 00 DB 00 00 00 00 00 00 00 00 00 00 00 03 02 00 00 08 55 56 56 56 55 56 55 56 55 55 55 56 EF]
//Write -> MC    => Seq: 0x6C NetFn/CMD cs-oem    SC_BMC_SET_SENSOR_INFO                                   :[C4 CC 20 6C 15 00 00 1B 1A FF FF 2C]
int processSC_BMC_SET_SENSOR_INFO(){

	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00; 	
	ackBuffer.ackPacket.payLoad[0] = 0x00;
	ackBuffer.ackPacket.payLoad[1] = 0x1b;
	ackBuffer.ackPacket.payLoad[2] = 0x1a;
	ackBuffer.ackPacket.payLoad[3] = 0xff;
	ackBuffer.ackPacket.payLoad[4] = 0xff;
	ackBuffer.ackPacket.payLoad[5] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 9);	 

	ackBuffer.ackPacket.reqDataPktSize = 12;


return 0;


}

//#define SC_BMC_GET_PROTOCOL_VERSION			0x302C
//Write -> iDrac => Seq: 0x08 NetFn/CMD CS-OEM    SC_BMC_GET_PROTOCOL_VERSION                              :[C0 20 70 08 2C 20 3C]
//Write -> MC    => Seq: 0x08 NetFn/CMD cs-oem    SC_BMC_GET_PROTOCOL_VERSION                              :[C4 CC 20 08 2C 00 30 7C]
int processSC_BMC_GET_PROTOCOL_VERSION(){

	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00; 	
	ackBuffer.ackPacket.payLoad[0] = 0x20;
	ackBuffer.ackPacket.payLoad[1] = 0x30;
	ackBuffer.ackPacket.payLoad[2] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 6);	 

	ackBuffer.ackPacket.reqDataPktSize = 9;


return 0;

}

int processGetID(){
//this is just a dummy responce... It will be filled by calls to teh Dbus layer i think)

	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00; 	
	ackBuffer.ackPacket.payLoad[0] = 0x20;
	ackBuffer.ackPacket.payLoad[1] = 0x81;
	ackBuffer.ackPacket.payLoad[2] = 0x03;
	ackBuffer.ackPacket.payLoad[3] = 0x00;
	ackBuffer.ackPacket.payLoad[4] = 0x02;
	ackBuffer.ackPacket.payLoad[5] = 0xdf;
	ackBuffer.ackPacket.payLoad[6] = 0xa2;
	ackBuffer.ackPacket.payLoad[7] = 0x02;
	ackBuffer.ackPacket.payLoad[8] = 0x00;
	ackBuffer.ackPacket.payLoad[9] = 0x00;
	ackBuffer.ackPacket.payLoad[10] = 0x01;
	ackBuffer.ackPacket.payLoad[11] = 0x00;
	ackBuffer.ackPacket.payLoad[12] = 0x43;
	ackBuffer.ackPacket.payLoad[13] = 0x00;
	ackBuffer.ackPacket.payLoad[14] = 0x00;	
	ackBuffer.ackPacket.payLoad[15] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 19);	 

	ackBuffer.ackPacket.reqDataPktSize = 22;


return 0;
}



int readPwm(void) {
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *m = NULL;
        sd_bus *bus = NULL;
        const char *path;
        int r;
        uint64_t pwm;

        /* Connect to the system bus */
        r = sd_bus_open_system(&bus);
        if (r < 0) {
                fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
                goto finish;
        }

        /* Issue the property call and store the respons message in m */
        r = sd_bus_get_property(bus,
                                "xyz.openbmc_project.RRC",
                                "/xyz/openbmc_project/sensors/temperature/final_pwm",
                                "xyz.openbmc_project.Sensor.Value",
                                "Value",
                                NULL,
                                &m,
                                "x");
        if (r < 0) {
                fprintf(stderr, "Failed to connect to read: %s\n", strerror(-r));
                goto finish;
        }

        /* Parse the response message */
        r = sd_bus_message_read(m, "x", &pwm);

        if (r < 0) {
                fprintf(stderr, "Failed to parse response message: %s\n", strerror(-r));
                goto finish;
        }

        printf("PWM is %d\n", (int) pwm);

finish:
        sd_bus_error_free(&error);
        sd_bus_message_unref(m);
        sd_bus_unref(bus);

        return r < 0 ? 0xAA55 : (int) pwm;

}

int processGetPWM(){

int pwmValue = readPwm();
//Write -> iDrac => Seq: 0x70 NetFn/CMD CS-OEM    SC_BMC_GET_PWM                                           :[C0 20 70 70 8C 94]
//Write -> MC    => Seq: 0x70 NetFn/CMD cs-oem    SC_BMC_GET_PWM                                           :[C4 CC 20 70 8C 00 39 00 AB]
	if(pwmValue == 0xaa55){
		processFailure(0xcb); // Requested Sensor, data, or record not present
	}else{	
		ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
		ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
		ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
		ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
		ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
		ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
		ackBuffer.ackPacket.completionCode = 0x00; 	
		ackBuffer.ackPacket.payLoad[0] = 0x20;
		ackBuffer.ackPacket.payLoad[1] = readPwm();
		ackBuffer.ackPacket.payLoad[2] = 0x00;
		ackBuffer.ackPacket.payLoad[3] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 7);	 

		ackBuffer.ackPacket.reqDataPktSize = 10;
	}

return 0;


}

int processGetPower(){

//Write -> iDrac => Seq: 0x74 NetFn/CMD GRP-EXTN  IPMI_DCMI_CMD_GET_POWER_READING                          :[B0 30 70 74 02 DC 01 00 00 3D]
//Write -> MC    => Seq: 0x74 NetFn/CMD grp-extn  IPMI_DCMI_CMD_GET_POWER_READING                          :[B4 DC 20 74 02 00 DC 54 00 07 00 20 01 41 00 A1 FF 41 59 E8 03 00 00 40 6C]
	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00; 	
	ackBuffer.ackPacket.payLoad[0] = 0xdc;
	ackBuffer.ackPacket.payLoad[1] = 0x54;
	ackBuffer.ackPacket.payLoad[2] = 0x00;
	ackBuffer.ackPacket.payLoad[3] = 0x07;
	ackBuffer.ackPacket.payLoad[4] = 0x00;
	ackBuffer.ackPacket.payLoad[5] = 0x20;
	ackBuffer.ackPacket.payLoad[6] = 0x01;
	ackBuffer.ackPacket.payLoad[7] = 0x41;
	ackBuffer.ackPacket.payLoad[8] = 0x00;
	ackBuffer.ackPacket.payLoad[9] = 0xa1;
	ackBuffer.ackPacket.payLoad[10] = 0xff;
	ackBuffer.ackPacket.payLoad[11] = 0x41;
	ackBuffer.ackPacket.payLoad[12] = 0x59;
	ackBuffer.ackPacket.payLoad[13] = 0xe8;
	ackBuffer.ackPacket.payLoad[14] = 0x03;
	ackBuffer.ackPacket.payLoad[15] = 0x00;
	ackBuffer.ackPacket.payLoad[16] = 0x00;
	ackBuffer.ackPacket.payLoad[17] = 0x40;
	ackBuffer.ackPacket.payLoad[18] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 22);	 

	ackBuffer.ackPacket.reqDataPktSize = 25;


return 0;


}
int readMAC(char *buffer) {
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *m = NULL;
        sd_bus *bus = NULL;
        const char *path;
	char *ptr;	
        int r;
        const char *macAddress;

        /* Connect to the system bus */
        r = sd_bus_open_system(&bus);
        if (r < 0) {
                printf("Failed to connect to system bus: %s\n", strerror(-r));
                goto finish;
        }


       /* Issue the property call and store the respons message in m */
        r = sd_bus_get_property(bus,
                                "xyz.openbmc_project.Network",
                                "/xyz/openbmc_project/network/eth0",
                                "xyz.openbmc_project.Network.MACAddress",
                                "MACAddress",
                                NULL,
                                &m,
                                "s");
        if (r < 0) {
                printf("Failed to connect to read: %s\n", strerror(-r));
                goto finish;
        }

        /* Parse the response message */
        r = sd_bus_message_read(m, "s", &macAddress);

        if (r < 0) {
                printf("Failed to parse response message: %s\n", strerror(-r));
                goto finish;
        }

        printf("MacAddress : <%s>\n",macAddress);
	//strtol(const char *str, char **endptr, int base)    
        buffer[0] = (char) strtol(macAddress, &ptr, 16);
        buffer[1] = (char) strtol(ptr+1, &ptr, 16);
        buffer[2] = (char) strtol(ptr+1, &ptr, 16);
        buffer[3] = (char) strtol(ptr+1, &ptr, 16);
        buffer[4] = (char) strtol(ptr+1, &ptr, 16);
        buffer[5] = (char) strtol(ptr+1, &ptr, 16);


finish:
        sd_bus_error_free(&error);
        sd_bus_message_unref(m);
        sd_bus_unref(bus);

        return 0 ;

}


#if 0
//I would like to use the debus but don't know how to resolve the value "d0a64e9"
//it is unique for each system 
int readIpAddress(void) {
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *m = NULL;
        sd_bus *bus = NULL;
        const char *path;
		char *ptr;	
        int r;
        const char *IpAddress;

        /* Connect to the system bus */
        r = sd_bus_open_system(&bus);
        if (r < 0) {
                printf("Failed to connect to system bus: %s\n", strerror(-r));
                goto finish;
        }


       /* Issue the property call and store the respons message in m */
        r = sd_bus_get_property(bus,
                                "xyz.openbmc_project.Network",
                                "/xyz/openbmc_project/network/eth0/ipv4/d0a64e9",
                                "xyz.openbmc_project.Network.IP",
                                "Address",
                                NULL,
                                &m,
                                "s");
        if (r < 0) {
                printf("Failed to connect to read: %s\n", strerror(-r));
                goto finish;
        }

        /* Parse the response message */
        r = sd_bus_message_read(m, "s", &IpAddress);

        if (r < 0) {
                printf("Failed to parse response message: %s\n", strerror(-r));
                goto finish;
        }

        printf("IpAddress is %s\n",IpAddress);
	//strtol(const char *str, char **endptr, int base)    
        r = strtol(IpAddress, &ptr, 10);
		printf("r = %02X\n",r);

        r = strtol(ptr+1, &ptr, 10);
        printf("r = %02X\n",r);

		r = strtol(ptr+1, &ptr, 10);
        printf("r = %02X\n",r);

        r = strtol(ptr+1, &ptr, 10);
        printf("r = %02X\n",r);



finish:
        sd_bus_error_free(&error);
        sd_bus_message_unref(m);
        sd_bus_unref(bus);

        return 0 ;

}
#else
//fall back for now....
int readIPAddress(char *buffer)
{
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    char *ptr;
	
    if (getifaddrs(&ifaddr) == -1) 
    {
        perror("getifaddrs");
        return EXIT_FAILURE;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) 
    {
        if (ifa->ifa_addr == NULL)
            continue;  
        s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

        if((strcmp(ifa->ifa_name,"eth0")==0)&&(ifa->ifa_addr->sa_family==AF_INET))
        {
	
            if (s != 0)
 
           {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                return EXIT_FAILURE;
            }
            printf("Interface : <%s>\n",ifa->ifa_name );
            printf("Address   : <%s>\n", host); 

        buffer[0] = strtol(host, &ptr, 10);
        buffer[1] = strtol(ptr+1, &ptr, 10);
	buffer[2] = strtol(ptr+1, &ptr, 10);
        buffer[3] = strtol(ptr+1, &ptr, 10);
			
        }
    }

    freeifaddrs(ifaddr);
    return EXIT_SUCCESS;
}
 
#endif

int processNicInfo(int which){

char netAddress[6];
//Write -> iDrac => Seq: 0xA4 NetFn/CMD Trans     IPMI_CMD_GET_LAN_CONFIG_PARA (5=MAC 3=IP)                :[30 B0 70 A4 02 01 03 00 00 E6]
//Write -> MC    => Seq: 0xA4 NetFn/CMD trans     IPMI_CMD_GET_LAN_CONFIG_PARA (5=MAC 3=IP)                :[34 5C 20 A4 02 00 11 C0 A8 11 C1 EF]
//Write -> iDrac => Seq: 0x38 NetFn/CMD Trans     IPMI_CMD_GET_LAN_CONFIG_PARA (5=MAC 3=IP)                :[30 B0 70 38 02 01 05 00 00 50]
//Write -> MC    => Seq: 0x38 NetFn/CMD trans     IPMI_CMD_GET_LAN_CONFIG_PARA (5=MAC 3=IP)                :[34 5C 20 38 02 00 11 10 98 36 B3 7C EC 9C]

	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00; 
	if(which == IP_ADDRESS){
		readIPAddress(netAddress);
		ackBuffer.ackPacket.payLoad[0] = 0x11;
		ackBuffer.ackPacket.payLoad[1] = netAddress[0];		//0xc0;
		ackBuffer.ackPacket.payLoad[2] = netAddress[1];		//0xa8;
		ackBuffer.ackPacket.payLoad[3] = netAddress[2];		//0x11;
		ackBuffer.ackPacket.payLoad[4] = netAddress[3];		//0xc1;
		ackBuffer.ackPacket.payLoad[5] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 9);	 
		ackBuffer.ackPacket.reqDataPktSize = 12;
	}else{
		readMAC(netAddress);
		ackBuffer.ackPacket.payLoad[0] = 0x11;
		ackBuffer.ackPacket.payLoad[1] = netAddress[0];		//0x10;
		ackBuffer.ackPacket.payLoad[2] = netAddress[1];		//0x98;
		ackBuffer.ackPacket.payLoad[3] = netAddress[2];		//0x36;
		ackBuffer.ackPacket.payLoad[4] = netAddress[3];		//0xb3;
		ackBuffer.ackPacket.payLoad[5] = netAddress[4];		//0x7c;
		ackBuffer.ackPacket.payLoad[6] = netAddress[5];		//0xec;
		ackBuffer.ackPacket.payLoad[7] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 11);	 
		ackBuffer.ackPacket.reqDataPktSize = 14;

	}

return 0;


}

int processSetPSUData(){

	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00;
	ackBuffer.ackPacket.payLoad[0] = reqBuffer.reqPacket.payLoad[1];	//subcommend
	ackBuffer.ackPacket.payLoad[1] = reqBuffer.reqPacket.payLoad[2];	//subcommand lenght
	ackBuffer.ackPacket.payLoad[2] = 0x00;
	ackBuffer.ackPacket.payLoad[3] = 0x00;
	ackBuffer.ackPacket.payLoad[4] = 0x00;
	 	
	ackBuffer.ackPacket.payLoad[5] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 9);	 

	ackBuffer.ackPacket.reqDataPktSize = 12;

	return(0);

}



int processFailure(char failureCode){

	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = failureCode; 	
	ackBuffer.ackPacket.payLoad[0] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 4);	 

	ackBuffer.ackPacket.reqDataPktSize = 7;

}

int processCheckSumError(int area){
//I can get away with this because I know they are talikg to me!

	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0xfe; 	
	ackBuffer.ackPacket.payLoad[0] = area; 	
	ackBuffer.ackPacket.payLoad[1] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 5);	 

	ackBuffer.ackPacket.reqDataPktSize = 8;

}



int processNetFun_CMD(){

int netFuncCmd = (reqBuffer.reqPacket.netFunc_LUN << 8) + reqBuffer.reqPacket.command;
int returnVal = 1;

	//printf("\t\tprocessNetFun_CMD()\n");
	switch(netFuncCmd){
		case IPMI_CMD_GET_DEVICE_ID: 
			//printf("\t\t\tIPMI_CMD_GET_DEVICE_ID\n");
			processGetID();
		break;
		case SC_BMC_GET_PWM:
			//printf("\t\t\tSC_BMC_GET_PWM\n");
			processGetPWM();
		break;
		case IPMI_DCMI_CMD_GET_POWER_READING:
			//printf("\t\t\tIPMI_DCMI_CMD_GET_POWER_READING\n");
			processGetPower();
		break;
		case IPMI_CMD_GET_LAN_CONFIG_PARA:
			if( reqBuffer.reqPacket.payLoad[1] == IP_ADDRESS || reqBuffer.reqPacket.payLoad[1] ==  MAC_ADDRESS){
				processNicInfo(reqBuffer.reqPacket.payLoad[1]);
			}else{
				printf("%02X Get Lan Command not supported yet\n",reqBuffer.reqPacket.payLoad[1]);
				goto NOT_SUPPORTED;
			}
		break;
		case BMCSubCommand:
			//printf("\t\t\tBMCSubCommand\n");
			if( reqBuffer.reqPacket.payLoad[1] == SetPSUDataForIBMGreyJoy){
				//printf("\t\t\t\tSetPSUDataForIBMGreyJoy\n");
				processSetPSUData();
			}else{
				printf("%02X Sub Command not supported yet\n",reqBuffer.reqPacket.payLoad[1]);
				goto NOT_SUPPORTED;
			}
		break;
		case SC_BMC_GET_PROTOCOL_VERSION:
			processSC_BMC_GET_PROTOCOL_VERSION();
		break;	
		case SC_BMC_SET_SENSOR_INFO:
			processSC_BMC_SET_SENSOR_INFO();
		break;	
		default:
			printf("%04X not supported yet\n",netFuncCmd);
NOT_SUPPORTED:
			processFailure(0xc1);	//not supported
			//this should load up an unsupport responce
			returnVal = 0;
	}			 

return returnVal = 1;
}

int writeDataACK(){
FILE *fptr;

	 //printf("\t\t\t\twriteDataACK()\n");
	
	 ackBuffer.ackPacket.lastMailBox = reqBuffer.reqPacket.lastMailBox;
	 fptr = fopen(FILE_NAME,"rb+");
	 fseek(fptr, ACK_DATA, SEEK_SET);
	 fwrite(&ackBuffer.ackPacket.netFunc_LUN,ackBuffer.ackPacket.reqDataPktSize,1,fptr);	//load up the answer
	 fseek(fptr, ACK_MAILBOX, SEEK_SET);
	 fwrite(ackBuffer.buffer,2,1,fptr);	
	 
	  	
	fclose(fptr);
	return 0;

}

int finishHandShake(){

FILE *fptr;

	 //printf("finishHandShake()\n");
	 ackBuffer.ackPacket.lastMailBox = reqBuffer.reqPacket.lastMailBox;
	 ackBuffer.ackPacket.reqDataPktSize = 0;
	 fptr = fopen(FILE_NAME,"rb+");
	 fseek(fptr, ACK_MAILBOX, SEEK_SET);
	 fwrite(ackBuffer.buffer,2,1,fptr);	
	fclose(fptr);
	//clear out all the buffers
	memset(ackBuffer.ackPacket.payLoad,0xff, 122);
	memset(reqBuffer.reqPacket.payLoad,0xff, 123);
	return 0;
}


//return value will be BC heart beat... later
long heartBeat = 0;
int upDateHeartBeat(){
FILE *fptr;


	 heartBeat++;
	 fptr = fopen(FILE_NAME,"rb+");
	 fseek(fptr, BMC_HB, SEEK_SET);
	 fwrite(&heartBeat,sizeof(heartBeat),1,fptr);
	  	
	fclose(fptr);
	return 44;

}

int initFile(){
FILE *fptr;


	 heartBeat++;
	 fptr = fopen(FILE_NAME,"rb+");
	 fseek(fptr, VERSION, SEEK_SET);
	 fwrite("001X",4,1,fptr);
	  	
	fclose(fptr);
	return 44;

}
int clearPacketAreas(){
#if 0
FILE *fptr;

//this is for debug, might cause a file area to get hammered on the way in....
	 fptr = fopen(FILE_NAME,"rb+");
	 fseek(fptr, ACK_DATA,SEEK_SET);
	 fwrite(ackBuffer.ackPacket.payLoad,1,sizeof(ackBuffer.ackPacket.payLoad),fptr);

	 fseek(fptr, REQ_DATA,SEEK_SET);
	 fwrite(reqBuffer.ackPacket.payLoad,1,sizeof(reqBuffer.ackPacket.payLoad),fptr);

	  	
	fclose(fptr);
#endif
	return 44;

}




int main()
{
int x;
int id;
int chkSumOK = 0;
int fd;

	id = fork();
	if(id == 0){	//this is the child

		//init the reg buffer....
		memset(reqBuffer.buffer,0,sizeof(reqBuffer.buffer));
		memset(ackBuffer.buffer,0,sizeof(ackBuffer.buffer));
		system(CREATE_SLAVE);
    	initFile();
    	catch_sigterm();
//setup to catch new signal..
        if (signal(SIGUSR1, sig_usr) == SIG_ERR) {
                fprintf(stderr, "Can't catch SIGUSR1: %s", strerror(errno));
                exit(1);
        }
        if (signal(SIGUSR2, sig_usr) == SIG_ERR) {
                fprintf(stderr, "Can't catch SIGUSR2: %s", strerror(errno));
                exit(1);
        }
        fd = open("/dev/scbmc_comm", O_RDWR);
        if (fd == -1)
        {
                printf("Error in opening file \n");
                exit(-1);
        }
        ioctl(fd,IOCTL_SC_BMC_COMM_REG);  //ioctl call
  		close(fd);
//
    	while(1){			
    		if(checkMailBox()){
				if(reqBuffer.reqPacket.lastMailBox != 0){
					//printf("\nPossible New Comamnd [%02x][%02x]\n\t",reqBuffer.reqPacket.lastMailBox,reqBuffer.reqPacket.reqDataPktSize);
					for(x=0;x<reqBuffer.reqPacket.reqDataPktSize;x++){
						if(!(x%16)){
							//printf("\n");
						}
						//printf("%02X ",reqBuffer.buffer[2+x]);
					}
					//printf("\n");
					if(chkSumOK = validateComamndData(&reqBuffer.reqPacket.BMCi2cAddress, reqBuffer.reqPacket.reqDataPktSize)){
						printf("reqBuffer checksum validation failed [%02x]\n",chkSumOK);
						processCheckSumError(chkSumOK);
						writeDataACK();

						//this will then load up an error packer
					}else{
						//the command is packet checksums out OK, start processin the command
						if(processNetFun_CMD()){
							writeDataACK();
						}
				   }

				}else{
					//printf("Comamnd Complete Sequence [%02x][%02x]\n\t",reqBuffer.reqPacket.lastMailBox,reqBuffer.reqPacket.reqDataPktSize);
					finishHandShake();
					clearPacketAreas();
					//printf("Task pid : %d\n",getpid());
				}
    		}
			upDateHeartBeat();

    		sleep(1);	//anthing less than 1 is a cpu hog....
    	}
    }else{
		//printf("Let the parrent die....\n");

    }
    return 0;

}

