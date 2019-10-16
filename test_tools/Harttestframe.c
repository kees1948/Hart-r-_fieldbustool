/*
* 
* This program is released under GNU Public License Version 3 or later.
* see gpl.txt for the full licence text or www.gnu.org/licences/#GPL
*
* Copyright 2010-2015   CWP Schoenmakers  kslinux@gmail.com
*
* This software is provided AS IS without any warranty. It is for
* field-device development purposes only and should NOT be used as an
* official Hart(r) master application in production environments.
* 
* Harttestframe, send a given string of comma separated hex values tot the Hart modem
*
* maybe with white space or comma
* and specify which byte in the output is to have a parity error
*
* with '##' as hex values, the program filters these from the output stream and
* restarts the Checksum calculation anew
*
* by default the program halts 20 mS after opening the port as most of times
* some glitch will be on RTS and as such the modem carrier on
* this delay can be suppressed
*
*/

#include "Hart.h"


void CarrierOn(void);
void CarrierOff(void);
void DOn(void);
void Doff(void);
int HartOpen(char *);
void OddParity(void);
void EvenParity(void);
void NoParity(void);
void Set1200Bd(void);
void Set2400Bd(void);
void Set4800Bd(void);
void Usage(void);


char HartPortName[128] = { 0 };
char Hartstring[256] = { 0 };
unsigned char CheckSum = 0;
int opt_errorbyte = 0;
int opt_frerror = 0;
int opt_gapbyte = 0;
int opt_sgapbyte = 0;
int opt_silentbyte = 0;
int opt_opdel = 0;
int charcount = 0;

struct pollfd hartfd;
struct termios tio;

int fdti_opt = 0;

//---------------------------------------------------------------------------------------
//
// 
//
int main(int argc, char **argv)
{
	int c, no_preambles = 0;
	unsigned char s;
	unsigned char t = 0;							// processed char
	unsigned char fe;
	char  *ptr;

	if(argc == 1){
		Usage();
		exit(1);
	}
//
    if (getenv("HART_PORT") != NULL)
        strncpy((char *) &HartPortName, getenv("HART_PORT"), 126);

    while ((c = getopt(argc, argv, "s:g:H:G:S:E:fnF:")) != -1) {
        switch (c) {
        case 'H':
            strncpy((char *) &HartPortName, optarg, 126);                                // Hart modem serial port name
            break;
		case 'S':																		// string to send
			strncpy((char*) &Hartstring, optarg, 254);
			break;
		case 'E':																		// PAR error
			opt_errorbyte = atoi(optarg);
			break;
		case 'F':																		// PAR error
			opt_frerror = atoi(optarg);
			break;
		case 'G':																		// physical gap
			opt_gapbyte = atoi(optarg);
			break;
		case 'g':																		// streaming gap (large)
			opt_silentbyte = atoi(optarg);
			break;
		case 's':																		// streaming gap (small)
			opt_sgapbyte = atoi(optarg);
			break;
		case 'n':																		// no wait after opening
			opt_opdel = 1;
			break;
        default:
            Usage();
            exit(2);                                                                     // invalid option/format
        }
    }
	
// debug
//    fprintf(stderr, "harttestframe:   using \"%s\" as HART port\n", HartPortName);
//    sleep(1);

    bzero(&hartfd, sizeof(struct pollfd));
    hartfd.fd = -1;
// open port
    if ((HartOpen(HartPortName)) < 0) {
        fprintf(stderr, "Can't open %s \n", HartPortName);
        exit(-1);
    }

	Set1200Bd();

// delay after opening?
	if(!opt_opdel)
		usleep(20000);																	// take time after opening glitch on RTS
// start carrier
	CarrierOn();
	ptr = Hartstring;
//
//
	charcount = strlen(ptr);
//
// main output loop
//
	while (charcount){
		sscanf((const char*)ptr, "%2x", &c);
		t++;																			// count pairs
		charcount -= 2;
		ptr += 2;
		s = (unsigned char) c;
		if(t == opt_gapbyte){			/// here RTS is visible signal
			CarrierOff();
			usleep(10000);
			CarrierOn();
		}
		if(t == opt_silentbyte){		// signal with DTR
			Doff();
			usleep(10000);
			DOn();		
		}
		if(t == opt_sgapbyte){			// signal with DTR
			Doff();
			usleep(3500);
			DOn();		
		}
// we assume preambles first, if not we passed them
		if(s != 0xff)
			no_preambles = 1;
// update checksum starting with delimiter
		if(no_preambles)
			CheckSum ^= s;
// special pattern to flag immediate start new frame
		if((strncmp(ptr, ",##", 3) == 0) || (strncmp(ptr, " ##", 3) == 0)){
			no_preambles = 0;
			CheckSum = 0;
			ptr += 3;
			charcount -= 3;
		}
// write byte to stream in selected parity
// insert parity error in stream 
		if(t == opt_errorbyte)
		{
			EvenParity();
			Doff();						// Signal with DTR
			write(hartfd.fd, &s, sizeof(char));
// wait until byte has been output from port
		usleep(9900);
		}
		else
		{
			OddParity();
			DOn();
// generate framing error
			if(t == opt_frerror)
			{
//
// framing error is difficult to create
// we emulate it with 1 byte at 2400 baud, NOP
// and one byte at 1200 baud in NOP
// $80, $01, which emulates $b0 with framing error
//
	int parm = 0;
				CheckSum ^= s;
				Doff();
				NoParity();
				Set2400Bd();
				fe = 0x80;
				write(hartfd.fd, &fe, sizeof(char));
				usleep(4000);
//				do
//				{
//					ioctl(hartfd.fd, TIOCSERGETLSR, &parm);
//				} while(!parm); 				
				Set1200Bd();
				fe = 0x00;
				write(hartfd.fd, &fe, sizeof(char));
				usleep(100);
				do
				{
					ioctl(hartfd.fd, TIOCSERGETLSR, &parm);
				} while(!parm); 				
				OddParity();
				DOn();
				if(charcount)
					charcount --;
				ptr++;																	// skip over separator
				continue;
			}
			else
			{
				write(hartfd.fd, &s, sizeof(char));
// wait until byte has been output from port
				usleep(9900);
			}
		}		
// does input string has comma. next pattern
		if(charcount)
			charcount --;
		ptr++;																	// skip over separator
	}	
// string done, append checksum
	t++;
// can be parity error even here
		if(t == opt_gapbyte){
			CarrierOff();										// signal with RTS
			usleep(10000);
			CarrierOn();
		}
		if(t == opt_silentbyte){
			usleep(10000);
		}
// write checksum byte
	if(t == opt_errorbyte){
		EvenParity();
		Doff();													// signal with DTR
		write(hartfd.fd, &CheckSum, sizeof(char));
	}
	else
	{
		OddParity();
		DOn();
		write(hartfd.fd, &CheckSum, sizeof(char));
	}
	usleep(10000);
// switch back to standard format, done
	OddParity();
	CarrierOff();
	DOn();
// all characters sent, finish
/*
	usleep(5000);
	while(poll(&hartfd, 1, 250) > 0)
	{
	  read(hartfd.fd, &s, 1);
	  fprintf(stdout, "%02x,", s);
	}
	fprintf(stdout,"\n");
*/
	return 0;
} 

//----------------------------------------------------------------------
//
// CarrierOff, this controls RTS to modem
//
void CarrierOff()
{
    int parm = 0;

    ioctl(hartfd.fd, TIOCMGET, &parm);
    parm &= ~TIOCM_RTS;                                                                  /* RTS OFF */
    ioctl(hartfd.fd, TIOCMSET, &parm);
	usleep(100);																		// this helps scheduler
}

//----------------------------------------------------------------------
//
// CarrierOn, this controls RTS to modem
//
void CarrierOn()
{
    int parm;
    ioctl(hartfd.fd, TIOCMGET, &parm);
    parm |= TIOCM_RTS;                                                                   /* RTS ON */
    ioctl(hartfd.fd, TIOCMSET, &parm);
	usleep(100);
}

//----------------------------------------------------------------------
//
// Open Hart modem port
//
int HartOpen(char *File)
{
    int parm;

    if (hartfd.fd == -1) {
        if ((hartfd.fd = open(File, O_RDWR)) < 0) {
            hartfd.fd = -1;
            return (-1);
        }
//
// set RTS to modem in receive 
        ioctl(hartfd.fd, TIOCMGET, &parm);
        parm &= ~TIOCM_RTS;                                                              // RTS off, go listen first 
        ioctl(hartfd.fd, TIOCMSET, &parm);
//
        tcgetattr(hartfd.fd, &tio);
        tio.c_cflag = CS8 | PARODD |  PARENB | CREAD | CLOCAL;                    // parity, baudrate
        tio.c_iflag = IGNBRK | INPCK;
        tio.c_lflag = 0;
        tio.c_oflag = 0;
        tio.c_cc[VMIN] = 0;                                                              // no input buffering
        tio.c_cc[VTIME] = 0;
        tcsetattr(hartfd.fd, TCSANOW, &tio);
// setup input polling on receive data
        hartfd.events = 0 | POLLIN;
        hartfd.revents = 0;
    }
    return (0);
}

//--------------------------------------------------------------------
//
// OddParity
//
void OddParity()
{
   	 tcgetattr(hartfd.fd, &tio);
	 tio.c_cflag &= ~CS7;
	 tio.c_cflag |= (PARENB | PARODD | CS8);
	 tcsetattr(hartfd.fd, TCSANOW, &tio);
	usleep(100);
}

//---------------------------------------------------------------------
//
// EvenParity
//
void EvenParity()
{
     tcgetattr(hartfd.fd, &tio);
     tio.c_cflag &= ~(PARODD | CS7);
	 tio.c_cflag |= (PARENB | CS8);
     tcsetattr(hartfd.fd, TCSANOW, &tio);
	usleep(100);
}

//---------------------------------------------------------------------
//
// No Parity
//
void NoParity()
{
     tcgetattr(hartfd.fd, &tio);
     tio.c_cflag &= ~(PARENB | CS7);
	tio.c_cflag |= CS8;
     tcsetattr(hartfd.fd, TCSANOW, &tio);
	usleep(100);
}

/*
//---------------------------------------------------------------------
//
// No par, 7 bit
//
void NoPar7Bit()
{
     tcgetattr(hartfd.fd, &tio);
     tio.c_cflag &= ~(PARENB | CS8);
     tio.c_cflag |= CS7;
     tcsetattr(hartfd.fd, TCSANOW, &tio);
	usleep(100);
}
*/

//---------------------------------------------------------------------
//
// 1200 Baud
//
void Set1200Bd()
{
     tcgetattr(hartfd.fd, &tio);
     tio.c_cflag &= ~CBAUD;
	 tio.c_cflag |= B1200;
     tcsetattr(hartfd.fd, TCSANOW, &tio);
	usleep(100);
}

//---------------------------------------------------------------------
//
// 2400 Baud
//
void Set2400Bd()
{
     tcgetattr(hartfd.fd, &tio);
     tio.c_cflag &= ~CBAUD;
	 tio.c_cflag |= B2400;
     tcsetattr(hartfd.fd, TCSANOW, &tio);
	usleep(100);
}

//---------------------------------------------------------------------
//
// 4800 Baud
//
void Set4800Bd()
{
     tcgetattr(hartfd.fd, &tio);
     tio.c_cflag &= ~CBAUD;
	 tio.c_cflag |= B4800;
     tcsetattr(hartfd.fd, TCSANOW, &tio);
	usleep(100);
}

//---------------------------------------------------------------------
//
// DTR off
//
void Doff()
{
	int parm;
    ioctl(hartfd.fd, TIOCMGET, &parm);
    parm &= ~TIOCM_DTR;                                                                  /* DTR OFF */
    ioctl(hartfd.fd, TIOCMSET, &parm);
	usleep(100);
}

//----------------------------------------------------------------------
//
// DTR on, 
//
void DOn()
{
    int parm;
    ioctl(hartfd.fd, TIOCMGET, &parm);
    parm |= TIOCM_DTR;                                                                   /* DTR ON */
    ioctl(hartfd.fd, TIOCMSET, &parm);
	usleep(100);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// usage, tell user how it should work
//
void Usage(void)
{
    fprintf(stderr,
            "harttestframe, Hart FSK frame test tool (DLL011 and DLL013) \n");
    fprintf(stderr,
            "option: -H <devicename>	IO port to use for Hart communication\n");
    fprintf(stderr,
            "option: -S	<xx,xx,xx >		(comma or space) separated string of HEX chars to be sent\n");
    fprintf(stderr,
			"        ## as hex code n string: restart CheckSum generation\n");
    fprintf(stderr,
            "option: -E	 <#>			which byte should have parity error\n");
    fprintf(stderr,
            "option: -G	 <#>			which byte should have a preceeding carrier (hard)gap\n");
    fprintf(stderr,
            "option: -g	 <#>			which byte should have a stream byte (soft)gap ( 10mS pause)\n");
    fprintf(stderr,
            "option: -s	 <#>			which byte should have a stream byte(soft)gap ( 4mS pause)\n");
    fprintf(stderr,
            "option: -n	 				suppress opening delay of 20 mS\n");
}

