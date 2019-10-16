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
 * Hart(r) Master emulator tool, assists in development of 
 * field devices.
 * It can be used in Primary or Secundary master mode
 * It can be used as Burst Mode monitor
 * It can be used as a Network monitor
 * The program complies with the timing specifications for the bus
 * arbritration between masters and for burstmode
 * It runs in a xterm or console window and uses curses for 
 * layout control
 * 
 * The program creates three threads, one for the transmission fucntions,
 * one for the receiving functions and one that maintains the timers
 * 
 */

#include	"Main.h"
#include 	"Hart.h"
#include	"ProcessFrames.h"
#include	"DataLink.h"

#define 	USE_CURSES
#define		ZERO		0
//#define DEBUGPORT_ON
//#define DEBUGDATAONLY
#define  DEBUGNAME "/dev/pts/14"

extern int Timer10mS, repmode;
extern volatile int FrameTimer;
volatile int BTimer = 0;                                                                 // burst timer
volatile int LQTimer = RT1_TIMES * 2;                                                        // link quiet timer
volatile int LGTimer = -1;                                                                // link grant timer
volatile int BuMoTimeout = 0;                                                            // clear burst listen after it
extern volatile BYTE WhoHasToken;
extern volatile BYTE BurstModeActive;
extern BYTE MasterAddressed;
extern int scanlinecount;
//
extern volatile int HartSerialState;
extern BYTE HartDelimiter;
extern BYTE HartCheckSum;
extern BYTE HartBrCheckSum;\
extern BYTE HartWrDataByteCount;
extern BYTE HartWrDataBuffer[];

extern struct timeval tv;
//
#ifdef HAS_DATABASE
MYSQL *db_conn;    
#endif
                                                                      // MySQL connection
BYTE WhoAmI = PRIMARY;                                                                   // Operating mode
int debugfd, broadcastflag;

pthread_t tx_thread, rc_thread, dll_thread;
pthread_mutex_t lt_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ft_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mr_mutex = PTHREAD_MUTEX_INITIALIZER;

char HartPortName[128] = { 0 };
char db_hostname[128] = { 0 };
char db_name[128] = { 0 };
char db_username[128] = { 0 };
char db_password[128] = { 0 };
char db_table[128] = { 0 };
char udp_hostname[128] = { 0 };
char udp_portnum[16] = { 0 };
char Home[256] = { 0 };
char ConfigName[270] = { 0 };
char tf_tablefile[270] = { 0 };

struct pollfd hartfd;
struct termios tio;

float version = 0.88;
char *operating = "Normal";
int burstlisten = 0, datalisten = 0;
int cmdrepflag = 0;
int nobreakflag = 0;
int polladdress = 0;
int transaction = 0;
int	cmdstringflag = 0;
int savey = 0;
int savex = 0;
int breakinput = 0;
int fdti_opt = 0;
int hwht_opt = 0;
int dump_opt = 0;
int timing_opt = 0;
int abstim_opt = 0;
int ana_opt = 0;
#ifndef HAS_DATABASE
int tf_opt = 1;
#else
int tf_opt = 0;
#endif

BYTE debugdata=0;
long timems = 0;
volatile int mainreceived = 0;

FILE *debugfile=NULL;
FILE *tf_file = NULL;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//
//
int main(int argc, char *argv[])
{
    int c;

#ifdef HAS_DATABASE
    if (getenv("HART_DBHOST") != NULL)                                                   // where is database located
        strncpy((char *) &db_hostname, getenv("HART_DBHOST"), 126);
#endif
    if (getenv("HART_PORT") != NULL)
        strncpy((char *) &HartPortName, getenv("HART_PORT"), 126);
	if (getenv("HOME") != NULL)
		strncpy((char *) &Home, getenv("HOME"), 254);

	sprintf(ConfigName, "%s/.hmt_config", Home);
	ReadConfig();
	sprintf(ConfigName, "./.hmt_config");
	ReadConfig();																		// try and use default first
#if defined (HAS_TABLEFILE)
    while ((c = getopt(argc, argv, "dBlsH:C:fSUtaAT")) != -1) {
#else
    while ((c = getopt(argc, argv, "dBlsH:C:fSUtaA")) != -1) {
#endif
      switch (c) {
        case 'H':
            strncpy((char *) &HartPortName, optarg, 126);                                // Hart modem serial port name
            break;
        case 's':
            WhoAmI = SECUNDARY;                                                          // assume a secundary master
            WhoHasToken = NOBODY;
            LQTimer = RT1_TIMES * 2;
            break;
 		case 'S':
 			cmdstringflag = 1;
 			break;
 		case 'U':
 			hwht_opt = 1;
 			break;
 		case 't':
 			timing_opt = 1;
 			break;
 		case 'T':
 			tf_opt = 1;
 			break;
		case 'A':
 			abstim_opt = 1;
 			break;
 		case 'a':
 			ana_opt = 1;
 			break;
        case 'l':
            datalisten = 1;                                                              // in bus listen mode only
            burstlisten = 0;
            dump_opt = 0;
            operating = "Listen";
            break;
        case 'B':
            burstlisten = 1;                                                             // in Burst Mode listen mode
            datalisten = 0;
            dump_opt = 0;
            operating = "BURST Listen";
            break;
        case 'C':
            strncpy(ConfigName, optarg, 268);                                            // alternative configfile
            if (ReadConfig() != 0) {
                fprintf(stderr, "option: -C, error reading config file\n");
                return (-1);
            }
            break;
        case 'f':
            fdti_opt++;                                                                  // adjust 
            break;
        case 'd':
            dump_opt++;
            datalisten = 0;
            burstlisten = 0;
            break;
        default:
            Usage();
            exit(2);                                                                     // invalid option/format
        }
    }
	
    fprintf(stderr, "master:   using \"%s\" as HART port\n", HartPortName);
    sleep(1);

#ifdef HAS_DATABASE    
    if (( (!strlen(db_hostname)) || (!strlen(db_table)) || (!strlen(db_name)) ) 
      && (!tf_opt)) {
        fprintf(stderr, "master: missing important database connect info\n");
        fprintf(stderr, "master: Config File not available? \n");
        return (-3);
    }
#endif

    if((!dump_opt) && (!tf_opt)){
#ifdef HAS_DATABASE
      if (mdbf_con(db_name) < 0) {
	    fprintf(stderr, "Database could not be opened\n");
	    exit(1);
      }
#endif
    }

#ifdef HAS_TABLEFILE
    if(tf_opt)
    {
		if((tf_file = fopen(tf_tablefile, "r")) == NULL)
		{
		  fprintf(stderr,"master: can't open table file name\n");
		  return 1;
		}
// just check if it there and contains at least one line
		if(ParseTableFile(0, 0, NULL) < 0)
		{
		  fprintf(stderr,"master: parse errors in table file at line %d\n", scanlinecount);
		  return 1;
		}
    }
#endif    

    bzero(&hartfd, sizeof(struct pollfd));
    hartfd.fd = -1;

    if ((HartOpen(HartPortName)) < 0) {
        fprintf(stderr, "Can't open %s \n", HartPortName);
        exit(-1);
    }

	if(dump_opt){
		tcgetattr(hartfd.fd, &tio);
        tio.c_iflag |=  IGNPAR ;              
		tcsetattr(hartfd.fd,TCSANOW, &tio);
	}
// 
// debug port is different serial port of which the hardware handshake
// signals are used to flag various states inside this program
// USB type serial ports may not be suitable for this!!
//
#if defined (DEBUGPORT_ON)
//    if ((debugfd = open(DEBUGNAME, O_RDWR | O_NONBLOCK)) < 0) {
 //       fprintf(stderr, "can't open debugport\n");
//        exit(-3);
// for pseudo tty only data transfer
#ifndef DEBUGDATAONLY
//        ioctl(debugfd, TCGETA, &tio);
//        // set port mode non echo       
//        tio.c_cflag = CS8 | PARODD | PARENB | B1200 | CREAD | CLOCAL;                    // parity, baudrate
//        tio.c_iflag = IGNBRK | INPCK;                                                    //| PARMRK;
//        tio.c_lflag = 0;
//        tio.c_oflag = 0;
//        tio.c_cc[VMIN] = 1;                                                              // no input buffering
//        tio.c_cc[VTIME] = 1;
//        ioctl(debugfd, TCSETA, &tio);
#endif
//    }
	if((debugfile = fopen(DEBUGNAME, "w+")) == NULL)
		exit(-3);
#endif
// catch the ^C interrupt
    signal(SIGINT, (void *) &break_command);                                             /* */

// create the mutexes for arbritration between the timers and threads
    pthread_mutex_unlock(&ft_mutex);
    pthread_mutex_unlock(&lt_mutex);
    pthread_mutex_unlock(&cd_mutex);
    pthread_mutex_unlock(&mr_mutex);
// put the curses mode on
#if defined(USE_CURSES)
    if (!dump_opt) {
        initscr();
//      raw();
        clear();                                                                         // clear and repaint screen
        refresh();
    }
#endif
	gettimeofday(&tv, NULL);
// create datalink layer thread, keeps in sync with datalink
//
     if (pthread_create(&dll_thread, NULL, ReadHartFrame, NULL)) {
        fprintf(stderr, "Creation of Read Thread failed\n");
        exit(-1);
    } 
// create transmit thread, handles user input and sends command 
// if bus arbritration allows
    if ((!burstlisten) && (!datalisten) && (!dump_opt))
        if (pthread_create(&tx_thread, NULL, WriteToHart, NULL)) {
            fprintf(stderr, "Creation of Write Thread failed\n");
            exit(-1);
        }
// create receive thread, handles the incoming frames and displays
// the data in a formatted fashion to the user
    if (pthread_create(&rc_thread, NULL, ReadFromHart, NULL)) {
        fprintf(stderr, "Creation of Read Thread failed\n");
        exit(-1);
    }
// spend rest of life in Timer()
	Timer(NULL);
	/* __NOTREACHED__ */
    return 0;
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
        tio.c_cflag = CS8 | PARODD | B1200 | PARENB | CREAD | CLOCAL;                    // parity, baudrate
        tio.c_iflag = IGNBRK | INPCK;
        tio.c_lflag = 0;
        tio.c_oflag = 0;
        tio.c_cc[VMIN] = 1;                                                              // no input buffering
        tio.c_cc[VTIME] = 0;
        tcsetattr(hartfd.fd, TCSANOW, &tio);
// setup input polling on receive data
        hartfd.events = 0 | POLLIN;
        hartfd.revents = 0;
    }
// setup bus timer              RT1     
    if (WhoAmI == PRIMARY)
        LQTimer = RT1_TIMEP;
    else
        LQTimer = RT1_TIMES;
    return (0);
}

//======================================================================
//
// WriteToHart, thread which collects user input, consults the database
// for the record layout and send the request
//
void *WriteToHart(void *arg)
{
    int c, result, len, binval;
    char mychar, *iptr, *optr;
    HT myht;
    HT *recptr = &myht;
    char buf[250];
    char hbuf[128];

    while (1) {                                                                          // do forever
        c = transaction = 0, breakinput = 0;
        broadcastflag = 1;

// getinput( x_pos, y_pos, len, prompt, buf, flag) 
// if in listen mode, skip data entering at all
        move(0, 0);
        printw("HART(r)  (%c)master tool version %2.2f in %s mode[%c] on %s\n",
               (WhoAmI == SECUNDARY) ? 'S' : 'P', version, operating, ((tf_opt == 0) ? 'D' : 'T'),
               (char *) &HartPortName);
        refresh();                                                                       // paint screen
		if(!cmdstringflag) {
	        if (GetInput(0, 2, 5, "  Command", buf, 1) == -2)                                // user input hart command
    	        p_end();                                                                     // end program
	        if (sscanf(buf, "%d", &c) == 1) {
				if (c < 0)
	                continue;
        	} else {
            	if (sscanf(buf, "%c", &mychar) == 1) {
                	recptr->cn = c;
                	PrHead(recptr);                                                          /* print status bits */
                	continue;
            	}
        	}
	        if ((result = GetInput(17, 2, 2, "TransAct.", buf, 1)) == -2)                    // transaction
   		         continue;
   		     if (result == -3)
   		         continue;
   		     if (sscanf(buf, "%d", &transaction) < 0) {
   		         move(2, 29);
   		         printw("%d", transaction);
   		         refresh();
   		     }
   		     if ((c == 11) || (c == 21) || (c == 73)) {
   		         if ((result = GetInput(34, 2, 2, "Brdcast y/n", buf, 1)) == -2)              // device or broadcast address
   		             continue;
   		         if (result == -3)
   		             continue;
   		         buf[0] = toupper(buf[0]);
   		         broadcastflag = ((strcmp(buf, "N")) ? 1 : 0);
   		     } else {
   		         if (c == 0) {
   		             if ((result = GetInput(33, 2, 2, "Poll. Addr.", buf, 1)) == -2)          // poll address
   		                 continue;
   		             if (result == -3)
   		                 continue;
   		             if ((sscanf(buf, "%d", &polladdress)) < 0) {
   		                 move(2, 47);
   		                 printw("%d", polladdress);
   		                 refresh();
   		             }
   		             if ((polladdress > 63) || (polladdress < 0))
   		                 continue;
   		         } else {
   		             move(2, 33);
   		             printw("               ");
   		             refresh();
   		         }
   		     }
   		     if ((result = GetInput(50, 2, 1, "Repeat, 'r,s' or CR", buf, 1)) == -2)          // repeat mode on?
   		         continue;
   		     if (result == -3)
   		         continue;
   		     buf[0] = toupper(buf[0]);
   		     cmdrepflag = ((strcmp(buf, "R")) ? 0 : 1);                                       // repeat mode
   		     nobreakflag = ((strcmp(buf, "S")) ? 0 : 1);                                      // don't break on error
   		     cmdrepflag = ((cmdrepflag | nobreakflag) ? 1 : 0);

	        do {
   		         move(4, 0);                                                                  /* remove previous data */
//
// lookup database to find details about frame build and input values
//
   		         if (ReadHartRecord(c, recptr, 0, DB_MATCH) != 0) {                           // recptr hold DB link
   		             move(4, 0);
   		             clrtobot();
   		             move(10, 0);
   		             printw("Could not access record for command!");                          // not in database!
   		             cmdrepflag = 0;
   		             continue;
   		         } else {
// ProcessRequest calls SendFrame
   		             if (ProcessRequest(recptr) != 0) {                                       // process the neccesary input  
   		                 cmdrepflag = 0;                                                      // and write the request
   		                 continue;
   		             }
   		         }
// reset from the repeatmode if setup that error would end it
   		         if (!nobreakflag)
   		             if (result < 0)
   		                 cmdrepflag = 0;
   		         refresh();
   		     } while (cmdrepflag);
// repaint the screen with current info
   		     refresh();
		} else {
  		    if ((result = GetInput(0, 1, 2, "Poll. Addr.", buf, 1)) == -2)          		// poll address
   		        p_end();
   		    if (result == -3)
   		        continue;
   		    if ((sscanf(buf, "%d", &polladdress)) < 0) {
   		        move(0, 12);
   		        printw("%d", polladdress);
   		        refresh();
   		    }
   		    if ((polladdress > 63) || (polladdress < 0))
   		        continue;			
   		    if ((result = GetInput(17, 1, 1, "Repeat, 'r,s' or CR", buf, 1)) == -2)          // repeat mode on?
   		        continue;
   		    if (result == -3)
   		        continue;
   		    buf[0] = toupper(buf[0]);
   		    cmdrepflag = ((strcmp(buf, "R")) ? 0 : 1);                                       // repeat mode
   		    nobreakflag = ((strcmp(buf, "S")) ? 0 : 1);                                      // don't break on error
   		    cmdrepflag = ((cmdrepflag | nobreakflag) ? 1 : 0);			
	        if (GetInput(0, 2, 250, "String", buf, 1) == -2)                                // user input hart command
    	        continue;
			len = strlen(buf);
			if((len >127) || (len == 0))
				continue;
//			
			iptr = buf;	
			optr = hbuf;
			*optr = '\0';
			while(len){
				if(*iptr == ' '){
					iptr++;
					len--;
					continue;
				}
				if((*iptr | 0x20) < '0'){
					continue;
				}
				if((*iptr | 0x20) <= '9'){
					*optr++ = *iptr++;
					len--;
					continue;
				}
				if((*iptr | 0x20) < 'a'){
					continue;
				}
				if((*iptr | 0x20) >'f'){
					continue;
				}
				*optr++ = *iptr++;
				len--;
			}
			do {
				iptr= hbuf;
				optr= (char*)HartWrDataBuffer;										//  buffer for data output
				len = strlen(hbuf);
				if(len & 1)													// odd len
					continue;
				while(len){
					sscanf(iptr, "%02x", &binval);
					*optr++ = (unsigned char) binval;
					iptr += 2;
					len -= 2;
				}
				HartWrDataByteCount = strlen(hbuf) / 2;
				WriteHartFrame();	
			} while (cmdrepflag);
		}
    }                                                                                    // while
}

//----------------------------------------------------------------------
//
// aborts loopmode, exits if not in command mode
//
void break_command()
{
    signal(SIGINT, (void *) &break_command);                                             // re-enable here for the next time
    cmdrepflag = 0;                                                                      // reset all flags
    repmode = 0;
    nobreakflag = 0;
    breakinput = 1;
    if ((burstlisten) || (datalisten) || (dump_opt)) {                                   // if listen only, exit program
        endwin();
        exit(0);
    }
}

//======================================================================
//
// Timer thread, handles all neccesary timers in 1 mS accuracy
// if value = -1 then timer is disabled
//
void *Timer(void *arg)
{
    while (1) {
        us_sleep(1000);                                                            // 1 mS step
        timems++;
        Timer10mS++;                                                                     // larger timing
        pthread_mutex_lock(&ft_mutex);
        if (FrameTimer > 0) {                                                            // did frame end or gap
            FrameTimer--;
			if(!dump_opt){
            	if (FrameTimer == 0) {
					HartSerialState = 0;
				}
            }
        }
        pthread_mutex_unlock(&ft_mutex);
//
        if (BuMoTimeout > 0) {
            BuMoTimeout--;
            if (BuMoTimeout == 0) {
                BurstModeActive = 0;                                                     // gone.......
                if (burstlisten) {
                    move(0, 0);
                    clrtobot();
                    printw
                        ("HART(r)  master tool version %2.2f in %s mode on %s\n",
                         version, operating, (char *) &HartPortName);
                    refresh();
                }
            }
        }
        pthread_mutex_lock(&lt_mutex);
        if (LQTimer > 0) {                                                               // Link Quiet timer
            LQTimer--;
            debugdata |= 2;
            if (LQTimer == 0) {
            }
        } else {
         	debugdata &= ~2;
		}
//
        if (LGTimer > 0) {                                                               // Link Grant timer
            LGTimer--;
            debugdata |= 1;
//                              setdebug(2+1);
        } else {
			debugdata &= ~1;
		}                                                                                // else
//                              setdebug(2+0);
        pthread_mutex_unlock(&lt_mutex);
//
//		setdebug(debugdata);
    }
}

//======================================================================
//
// readFromHart, thread which collects input data and decodes
// it, presents it according decription in database
//
void *ReadFromHart(void *arg)
{
    int ft = 0, bc = 0, lmr;
    char blinker[] = { '|', '/', '-', '\\' };                                            //
    char beat = '#';
    int toggleb = 0;
    int mxpos = 4;
//
// do forever
//
    while (1) 
	{
//        ft = ReadHartFrame();
		pthread_mutex_lock(&mr_mutex);
		mainreceived = 2;
		pthread_mutex_unlock(&mr_mutex);
		do 
		{
			pthread_mutex_lock(&mr_mutex);
			lmr = mainreceived;
			pthread_mutex_unlock(&mr_mutex);
			us_sleep(1000);
		} while (lmr != 1);
        if (dump_opt)
            continue;
//
        move(1, 0);
        if (toggleb & 1)
            printw("%c", beat);
        else
            printw(" ");
        toggleb++;
        move(savey, savex);
        refresh();
//
        switch (HartDelimiter & HART_DEL_MASK) {
        case HART_D_ACK:
            if ((ft != 0) || (HartCheckSum)) {
                move(1, mxpos);
                if (!datalisten) {
                    printw("Error in receive:%d chksm:%02x BM:%d", ft,
                           HartCheckSum, BurstModeActive);
                    move(savey, savex);
                    refresh();
                }
                break;
            }
            if(MasterAddressed != WhoAmI)
            	break;
            move(1, mxpos);
            printw("Receive OK                        ");
            refresh();
//#if defined(USE_CURSES)
            if (datalisten) {
                move(0, 0);
                clrtobot();
                printw
                    ("HART(r)  master tool version %2.2f in %s mode on %s\n",
                     version, operating, (char *) &HartPortName);
                move(savey, savex);                                                      // locate cursor
                ProcessResponse();
            } 
			else 
			{
                move(1, 2);
                bc++;
                bc &= 3;
                printw("%c", blinker[bc]);
                move(savey, savex);
                ProcessResponse();
                move(savey, savex);
            }
            if ((cmdrepflag) || (nobreakflag))
                move(0, 0);
//#endif
            break;
        case HART_D_BACK:                                                               // Burst Ack frame, 
            if ((ft != 0) || (HartCheckSum != 0))
                break;
//#if defined(USE_CURSES)
            if (burstlisten) {                                                           // show data if in Burst Listen mode
                move(0, 0);
                clrtobot();
                printw
                    ("HART(r)  master tool version %2.2f in %s mode on %s\n",
                     version, operating, (char *) &HartPortName);
                move(0, 0);                                                              // locate cursor
                ProcessBrResponse();
                move(1, 3);
                bc++;
                bc &= 3;
                printw("%c", blinker[bc]);
                move(savey, savex);
                refresh();
            }
            break;
        }
#if defined(USE_CURSES)
        refresh();
#endif
    }                                                                                    // while
}

#ifdef HAS_DATABASE
//----------------------------------------------------------------------
//
// mdbf_con, open database connection here
//
int mdbf_con(char *db_name)
{

    if ((db_conn = mysql_init(NULL)) == ZERO)
        return (-1);

    db_conn = mysql_real_connect(db_conn,                                                // MYSQL *mysql
                                 db_hostname,                                            // const char *host
                                 db_username,                                            // const char *user
                                 db_password,                                            // const char *password
                                 db_name,                                                // const char *db
                                 ZERO,                                                   // unsigned int port
                                 NULL,                                                   // const char *unix_socket
                                 ZERO);                                                  // unsigned long client_flag
    if (db_conn == NULL)
        return (-1);
    return (0);
}
#endif

//----------------------------------------------------------------------
//
// issue a prompt and input the data 
//
// xpos, ypos, #chars of input, prompt string, input buffer, if input can
// return with exit flag
//
int GetInput(int x, int y, int l, char *prompt, char *buf, int flag)
{

    int newx, c = '0';
    int ch = ' ';
    int n = 0;
    int pl = strlen(prompt) + 2;

    savex = x;
    savey = y;
    newx = savex + pl;
    noecho();                                                                            // don't echo typed chars

    move(savey, savex);
    printw("%s :", prompt);
    for (c = 0; c < l; c++)
        buf[c] = '\0';
//
    move(savey, newx);
    for (c = 0; c <= l; c++)
        addch(' ');
    refresh();
//
    do {
        move(savey, newx + n);
        savex = newx + n;
        savey = y;
        refresh();
        if ((ch = getch()) == 0)
            continue;
        if (breakinput) {                                                                // someone hit ^C
            breakinput = 0;
            echo();
            return (-3);
        }
        if ((ch == '!') && (flag == 1))                                                  // '!' as commandno means EXIT program
            return (-2);
        if ((ch == '\010') || (ch == 0x7f)) {                                            // delete/backspace
            if (n) {
                buf[n] = ' ';                                                            // overwrite with space
                n--;
                move(savey, newx + n);
                addch(' ');
                refresh();
            }
            continue;
        }
        if (n < l) {
            if (ch == '\n')
                buf[n] = '\0';
            else {
                buf[n] = (char) ch;
                move(savey, newx + n);
                printw("%c", ch);
                refresh();
                n++;
            }
        }
    } while (ch != '\n');
    echo();                                                                              // restore echo mode
    return (0);                                                                          // successful return
}


//----------------------------------------------------------------------
//
// p_end, leave program
//
void p_end()
{
#ifdef HAS_DATABASE
	if(!dump_opt)
    	mysql_close(db_conn);                                                                // sign off to database
#endif
    endwin();                                                                            // xterm/console back to normal
    exit(0);
}

//----------------------------------------------------------------------
// 
// use handshake lines of another port
// to signal status to the outside world
// (DTR can be used too)
//
#if defined (DEBUGPORT_ON)
void setdebug(char *flag)
{
    int parm;
	char lchar = (char) flag + '0';
	
#ifdef DEBUGDATAONLY
	fprintf(debugfile, ":%s\n", flag);
	fflush(debugfile);

//	write(debugfd, &lchar, 1);
//	lchar = '\n';
//	write(debugfd, &lchar, 1);
#else
    ioctl(debugfd, TIOCMGET, &parm);
//    if (flag & 2) {
	if(MasterAddressed == PRIMARY){
        parm |= TIOCM_RTS;                                                           /* RTS on */
    } else {
        parm &= ~TIOCM_RTS;                                                          /* RTS off */
    }
    if (flag & 1) {
        parm |= TIOCM_DTR;                                                           /* RTS on */
    } else {
        parm &= ~TIOCM_DTR;                                                          /* RTS off */
    }
    ioctl(debugfd, TIOCMSET, &parm);
#endif
}
#else
void setdebug(char *flag)
{
}
#endif // DEBUGPORT_ON

// table to translate strings to its parameter

typedef struct
{
    char *name;
    void *arg;
} CF;

CF xtable[] = {
    {"db_hostname", &db_hostname},
    {"HartPortName", &HartPortName},
    {"db_name", &db_name},
    {"db_username", &db_username},
    {"db_password", &db_password},
    {"db_table", &db_table},
    {"udp_hostname", &udp_hostname},
    {"udp_portnum", &udp_portnum},
    {"tf_tables", &tf_tablefile},
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

	fprintf(stderr,"master trying %s as config file\n", (char*)&ConfigName);	
    if ((config = fopen((char*)&ConfigName, "r")) == NULL)
        return (-1);

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
        fprintf(stderr, "Syntax error, line %d\n", linenum);
        continue;
    }
    fclose(config);
    return (0);
}

//-----------------------------------------------------------------------------------------
//
// us_sleep
//
//-----------------------------------------------------------------------------------------
int us_sleep(int s)
{
        struct timespec ts = {0};

        ts.tv_nsec = (long) s * US_SLEEP_1MS;

        return nanosleep(&ts, NULL);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// usage, tell user how it should work   BlsH:C:f
//
void Usage(void)
{
    fprintf(stderr,
            "master, Hart master emulator tool, can work as Primary or Secondary master\n");
    fprintf(stderr, "exit program with '!' in command field\n");
    fprintf(stderr,
            "option: -B			Burst mode listen only, (mutually exclusive with -l)\n");
    fprintf(stderr,
            "option: -l			Data listen only, (mutually exclusive with -B)\n");
    fprintf(stderr,
            "option: -t			Show timing info in dump\n");
    fprintf(stderr,
            "option: -A			Show absolute timing info of delimiter byte in dump\n");
#ifdef HAS_TABLEFILE
    fprintf(stderr,
            "option: -T			Use table file 'tf_tablefile' instead of MySQL database");
#endif
    fprintf(stderr,
            "option: -d			dump data in HEX \n");
    fprintf(stderr,
            "option: -a			Show Analyse info in dump\n");
    fprintf(stderr,
            "option: -U			Hardware Handshake timed  (some USB serial dongles)\n");
    fprintf(stderr,
            "option: -C <filename>	Alternative configuration file, default is ~/.hmt_config and ./.hmt_config \n");
    fprintf(stderr,
            "                   beware! the configfile parms take precedence, order of command line options is important! \n");
    fprintf(stderr,
            "option: -H <devicename>	IO port to use for Hart communication\n");
    fprintf(stderr,
            "option: -s			Perform as SECONDARY master\n");
    fprintf(stderr,
            "option: -f			Special mode for fdti usb chip\n");
}
