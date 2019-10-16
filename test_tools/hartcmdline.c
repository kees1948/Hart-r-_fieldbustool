/*
 * 
 * This program is released under GNU Public License Version 3 or later.
 * see gpl.txt for the full licence text or www.gnu.org/licences/#GPL
 *
 * It is een assistance tool in testen the filed device. As parameters
 * are delivered as 2 character hex is it easy to create a script layer
 * around it. It can be used for automated testing in production
 * environments.
 * 
 * 
 * Copyright 2010-2014   CWP Schoenmakers  kslinux@gmail.com
 * 
 * This software is provided AS IS without any warranty. It is for 
 * field-device development purposes only and should NOT be used as an
 * official Hart(r) master application in production environments.
 * 
 */

#include    <stdio.h>
#include    <fcntl.h>
#include    <termio.h>
#include 	<string.h>
#include    <sys/types.h>
#include    <sys/stat.h>
#include 	<sys/poll.h>
#include 	<errno.h>
#include    <unistd.h>
#include    <stdlib.h>
#include	"hartcmdline.h"


char        HartPortName[128]   = { 0 };
char        Home[256]           = { 0 };
unsigned char Hartstring[256]   = {0};
char        ConfigName[270]     = { 0 };
char        hartaddressbytes[5] = {0};
int         writecharactertime  = 0;
int         fdti_opt            = 0;
int         hwht_opt            = 0;
int         nolsr_opt           = 0;
int         verb_opt            = 0;

struct      pollfd hartfd       = {0};
struct      termios tio;

char hartaddressfilename[128]   = ".hartdevad";
char *hartcommandstring         = NULL;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//
//
int main(int argc, char *argv[])
{
    int c, result = 0, afdn = -1;

    if (getenv("HART_PORT") != NULL)
        strncpy((char *) &HartPortName, getenv("HART_PORT"), 126);
	if (getenv("HOME") != NULL)
		strncpy((char *) &Home, getenv("HOME"), 254);

	sprintf(ConfigName, "%s/.hmt_config", Home);
	ReadConfig();
	sprintf(ConfigName, "./.hmt_config");
	ReadConfig();																		// try and use default first

    while ((c = getopt(argc, argv, "H:C:fD:Uv")) != -1) {
        switch (c) {
        case 'H':
            strncpy((char *) &HartPortName, optarg, 126);                                // Hart modem serial port name
            break;
        case 'C':
            strncpy(ConfigName, optarg, 268);                                            // alternative configfile
            if (ReadConfig() != 0) {
                fprintf(stderr, "option: -C, error reading config file\n");
                return (-1);
            }
            break;
        case 'U':
            hwht_opt = 1;
            break;
        case 'D':
            strncpy((char*) &Hartstring, optarg, 254);
            break;
        case 'f':
            fdti_opt++;                                                                  // adjust 
            break;
		case 'v':
			verb_opt++;
			break;
        default:
            Usage();
            exit(2);                                                                     // invalid option/format
        }
    }
	
    hartfd.fd = -1;
//
// read saved addressbytes from previous communication (command 0)
//
    if((afdn = open(hartaddressfilename, O_RDONLY)) < 0)
	{
		afdn = open(hartaddressfilename, O_CREAT, 0600);
		write(afdn, &hartaddressbytes, 5);
		close(afdn);
	} 
	else
	{
	    if( read(afdn, &hartaddressbytes, 5) != 5)
			ReportError(-7);
    	close(afdn);
	}
//
// open Hart port
//    
    if ((HartOpen(HartPortName)) < 0) {
        fprintf(stderr, "Can't open %s \n", HartPortName);
        ReportError(-1);
    }
//
// send cmd frame to Hart device
//
    SendString();
//
// process response frame from Hart device
//    
    ReceiveString();
//
// done
//    
    return(result);
}

// long and short header
const char header  = 0x82;
const char header0[] = {0x02,0x80,0x00,0x00};

//
// build request frame from commandline args
// use address bytes if appropriate
//
void SendString()
{
unsigned char   outbuf[256] = {0xff};
int             charcount   = -1;
unsigned char   *ptr        = Hartstring;
unsigned char   *wptr ;  
unsigned char   checksum    = 0;
int             t, mocheck  = 0, c;


	memset(outbuf, 0xff, sizeof(outbuf));                               // fill buffer with preambles
	hartaddressbytes[0] |= 0x80;							            // set from primary master
  
    if(*ptr)                                                            // long frame?
	{
		wptr = outbuf + 5;	                                            // send 5 preambles
		strncpy((char*)wptr, &header, sizeof(char));                           // build header
		wptr++;
		strncpy((char*)wptr, hartaddressbytes, 5);                             // with address bytes
		wptr += 5;
		t = 11;
      	charcount = strlen((char*)ptr);
//
// data string is 2 hex char with separator
//
	    while(charcount){
	      	sscanf((const char*)ptr,"%02x", &c);
    	  	charcount -= 2;
      		ptr += 2;
      		*wptr++ = (unsigned char) c;
      		t++;
	      	if(charcount){			                                    // trim separator
				charcount--;
				ptr++;
    	  	}
    	}
	}
	else                                                                // short frame
	{
		wptr = outbuf + 20;                                             // send 20 preambles
		wptr = (unsigned char*) strncpy((char*)wptr, (char*)&header0, sizeof(header0));         // build header
		t = 24;
	}
//
// build checksum, start with first non-preamble
//
	for (c = 0; c < t ; c++)
	{
      	if(outbuf[c] != 0xff)
			mocheck = 1;
      
      	if(mocheck)
			checksum ^= (unsigned char) outbuf[c];
	}	
//
// start sending
//
    CarrierOn();
// write framebuffer
    write(hartfd.fd, &outbuf, t);
// write checksumbyte
    write(hartfd.fd, &checksum,1);
// delay for all transmitted characters
    writecharactertime = 9200 * (t+1);
// end transmission, go receive
    CarrierOff();
}

//
// process received frame
//
void ReceiveString()
{
unsigned char   *ptr            = Hartstring;
unsigned char   readbuf[256];
unsigned char   *rptr           = readbuf;
unsigned char   c;
int             i, j, docheck   = 0, afdn = -1, preamblecount = 0, reccnt = 0;
unsigned char   checksum        = 0;

//
// wait RT1_TIME for response,
//
	if(poll(&hartfd, 1, 250) < 0)                                       // 250 mS
	{
		ReportError(-6);
	}
//
// read data into buffer
//
	while(poll(&hartfd, 1, 20) > 0)
	{
		read(hartfd.fd, &c, 1);
		*rptr++ = c;
// check overflow
    	if(( rptr - readbuf) > 250)
		{
			ReportError(-2);
		}
	}
//    
// we have read frame, check it
//
	i = rptr - readbuf;								// total bytes in buffer
// check if checksum is GOOD	
	for (j = 0; j < (i - 1) ; j++)
	{
		if(docheck == 0)
		{
			if(readbuf[j] == 0xff)
			{
				preamblecount++;
				continue;
			}
			else if(((readbuf[j] & 0x07) == 0x06) && (preamblecount >= 2))
			{
				docheck = j;			                                // reminder SOH
				checksum = 0;											// re-init checksum
				if(readbuf[j] & 0x80)
				{
					reccnt = readbuf[j + 7] + 8;						// long frame
				}
				else
				{ 
					reccnt = readbuf[j + 3] + 4;						// short frame
				}
			}      	
			else
				preamblecount = 0;										// no valid header yet
		}
	}
//
// now process frame data
//
	for( j = 0; j <= reccnt; j++)
	{
		checksum ^= (unsigned char) readbuf[j + docheck];
	}
//
	if(checksum)
	{
		ReportError(-3);
	}
//
// valid frame read
//
	if(!(*ptr))
	{
		if(readbuf[docheck] != 0x06)                                    // short frame response
		{
			ReportError(-4);
		}
		hartaddressbytes[0] = readbuf[docheck + 7] & 0x3f;              // save address info
		hartaddressbytes[1] = readbuf[docheck + 8];	                    //
		hartaddressbytes[2] = readbuf[docheck + 15];                    //
		hartaddressbytes[3] = readbuf[docheck + 16];                    //
		hartaddressbytes[4] = readbuf[docheck + 17];                    // in file for next cycle
//
		afdn = open(hartaddressfilename, O_CREAT|O_RDWR, 00644);
		write(afdn, &hartaddressbytes, 5);
		close(afdn);
//
		docheck += 2;
		i -= docheck;
	} 
	else
	{
		if(readbuf[docheck] != 0x86)                                    // long frame response
		{
			ReportError(-5);
		}
		docheck += 6;		                                            // skip over header    
		i -= docheck;
	}
//
// print data part, start with cmd, datacount, resp1, resp2, data.....
//
	for(j = 0 ; j < i ; j++)
	{
		printf("%02x,", readbuf[docheck + j]);
	}
	printf("\n");
// done
}

// arry to show error in ascii
const char *errmsg[][32]={
	{"Port open error"},
	{"Read buffer overflow"},
	{"Checksum error"},
	{"Unknown short frame type"},
	{"Unknown long frametype"},
	{"No reply"},
	{"Read Error in .hartdevad"},
	{"min8"},
};
	
//
// resport error in various ways, depending on flags
//
void ReportError(int error)
{
	int myerror = abs(error) - 1;

	if(verb_opt == 0)
		exit(-1);
	if(verb_opt == 2)
	{
		fprintf(stderr, "%s\n", *errmsg[myerror & 0x07]);
	}	
	exit(error);
}

//----------------------------------------------------------------------
//
// Open Hart modem port
//
int HartOpen(char *File)
{
    int parm;
    
	if(verb_opt)
	{
		fprintf(stderr, "Try to open port: %s \n", File);
	}
    if (hartfd.fd == -1) {
        if ((hartfd.fd = open(File, O_RDWR)) < 0) {
            hartfd.fd = -1;
            return (-1);
        }
//
// set RTS to modem in receive 
//
	ioctl(hartfd.fd, TIOCMGET, &parm);
        parm &= ~TIOCM_RTS;                                                              // RTS off, go listen first 
        ioctl(hartfd.fd, TIOCMSET, &parm);
	//
        tcgetattr(hartfd.fd, &tio);
        tio.c_cflag = CS8 | PARODD | B1200 | PARENB | CREAD | CLOCAL;                    // parity, baudrate
        tio.c_iflag = IGNBRK | IGNPAR; //INPCK;
        tio.c_lflag = 0;
        tio.c_oflag = 0;
        tio.c_cc[VMIN] = 1;                                                              // no input buffering
        tio.c_cc[VTIME] = 0;
        tcsetattr(hartfd.fd, TCSANOW, &tio);
// setup input polling on receive data
        hartfd.events = 0 | POLLIN;
        hartfd.revents = 0;
    }
    return (0);
}

//----------------------------------------------------------------------
//
// CarrierOn, this controls RTS to modem
//
void CarrierOn()
{
    int parm;
    
    writecharactertime = 0;
    ioctl(hartfd.fd, TIOCMGET, &parm);
    parm |= TIOCM_RTS;                                                                   /* RTS ON */
    ioctl(hartfd.fd, TIOCMSET, &parm);
}

//----------------------------------------------------------------------
//
// CarrierOff, this controls RTS to modem
//
void CarrierOff()
{
    int parm = (nolsr_opt) ? 1 : 0;

	usleep(2000);														// force context switch
    if(hwht_opt){
        usleep(writecharactertime + 2000);
    } else {
        do {
            if (fdti_opt)
                usleep(9000);
			usleep(1000);
			if(!nolsr_opt)
            	ioctl(hartfd.fd, TIOCSERGETLSR, &parm);                 // make sure ALL bytes are written
        } while (!parm);
        if (fdti_opt)
            usleep(24000);
    }                                                                   // fdti driver has problems with TIOGETLSR
    ioctl(hartfd.fd, TIOCMGET, &parm);
    parm &= ~TIOCM_RTS;                                                 /* RTS OFF */
    ioctl(hartfd.fd, TIOCMSET, &parm);
}

//
// for config file settings
// table to translate strings to its parameter
//
typedef struct
{
    char *name;
    void *arg;
} CF;                                                                   // config item

CF xtable[] = {
    {"HartPortName",    &HartPortName},
    {"HartAdrresFile",  &hartaddressfilename},
    {NULL, NULL},
};

//-----------------------------------------------------------------------------
//
// read config file
//
int ReadConfig()
{
    char line[128];
    int linenum = 0;
    FILE *config;
    char arg1[128];
    char arg2[128];
    char p;
    CF *xptr;

    if ((config = fopen((char*)&ConfigName, "r")) == NULL)
        return (-1);
	if(verb_opt)
		fprintf(stderr,"master trying %s as config file\n", (char*)&ConfigName);	

    while (fgets(line, 128, config) != NULL) {
        linenum++;
        if ((line[0] == '#') || (strlen(line) < 2))
            continue;
//
        if (sscanf
            (line, "%s %c %s", (char *) &arg1, (char *) &p,
             (char *) &arg2) == 3) {
// seperator OK
            if (p == '=') {
// search string match in table
                for (xptr = (CF *) & xtable[0]; xptr->name != NULL; xptr++) {
                    if (strcmp((char *) xptr->name, (char *) &arg1) == 0)
                        break;
                }
                if (xptr->name != NULL) {
// use address from table to set the value
                    strcpy((char *) xptr->arg, (char *) &arg2);
                    continue;
                }
            }
        }
//        fprintf(stderr, "Syntax error, line %d\n", linenum);
        continue;
    }
    fclose(config);
    return (0);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// usage, tell user how it should work   BlsH:C:f
//
void Usage(void)
{
    fprintf(stderr,
            "info:   hartcmdline, primary master commandline tool (for scripting)\n");
    fprintf(stderr,
			"info:   uses '.hmt_config' from home and current directory as well HART_PORT(env) for settings\n");
    fprintf(stderr,
			"info:   when applied without -D argument it tries to issue cmd 0 and save the address info in '.hartdevad'\n");
    fprintf(stderr,
			"info:   this should be done at first connection, else communication will fail!\n\n");
    fprintf(stderr,
            "option: -D <string>    supply separated hex bytes.  (i.e. cmd 3 = 03,04,00,01,02,03) \n");
    fprintf(stderr,
            "          format:      cmd byte, datalength, request databyte(s)  (checksum will be appended automatically) \n");
    fprintf(stderr,
            "             ===>>>    NO data bytes implies Command 0, return device ID \n");
    fprintf(stderr,
            "option: -U             Hardware Handshake timed  (i.e. USB serial dongles)\n");
    fprintf(stderr,
            "option: -C <filename>	Alternative configuration file, default is ~/.hmt_config and ./.hmt_config \n");
    fprintf(stderr,
            "                       beware! the configfile parms take precedence, order of command line options is important! \n");
    fprintf(stderr,
            "option: -H <devicename> IO port to use for Hart communication\n");
    fprintf(stderr,
            "option: -f             Special mode for fdti usb chip\n");
}
