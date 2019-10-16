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

#include    "Hart.h"
#include    "ProcessFrames.h"
#include    "DataLink.h"
#include    "Main.h"

unsigned short h_devtype = 0;
unsigned short b_devtype = 0;
extern int broadcastflag;
extern int cmdrepflag;
#ifdef HAS_DATABASE
extern MYSQL *db_conn;
#endif

extern char *db_table;
extern int polladdress;
extern int transaction;
extern int tf_opt;
extern FILE *tf_file;

extern volatile BYTE BurstModeActive;
extern BYTE HartDelimiter;
extern BYTE HartAddressBytes[];
extern BYTE HartCommandByte;
extern BYTE HartDataByteCount;
extern BYTE HartDataBuffer[];
extern BYTE HartCheckSum;
//
extern BYTE HartWrSerialState;
extern BYTE HartWrDelimiter;
extern BYTE HartWrAddressBytes[];
extern BYTE HartWrCommandByte;
extern BYTE HartWrDataByteCount;
extern BYTE HartWrNumPreambles;
extern BYTE HartWrDataBuffer[];
//
extern BYTE HartBrAddressBytes[];
extern BYTE HartBrCommandByte;
extern BYTE HartBrDataByteCount;
extern BYTE HartBrDataBuffer[];
extern BYTE HartBrCheckSum;
//
int repmode = 0;
unsigned char cmd48data[25] = {[0 ... 24] = 0x00 };
int scanlinecount = 0;

SI Slaves[MAXPOLL + 1];

union
{
    unsigned long ul;
    float uf;
    char uc[4];
} uuv;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ProcessRequest, build XMIT frame from user input
//
int ProcessRequest(HT * ptr)
{
    HT localinfo;
    HT *liptr = &localinfo;
    int xpos = 0, ypos = 5;
    int ixpos = 0, iypos = 0;
    unsigned char *aptr;
    char mybuf[128], formbuf[128];
    int parm = 1, i, j, k;
    int fc = 0, fl = 0, index = 0, escape = 0;

    bzero(&mybuf, sizeof(mybuf));
// in repeat mode, we use user input from last entry again
    if (cmdrepflag && (repmode == 1)) {
        if (ptr->cn == 0)
            HartWrDelimiter = HART_D_STX;
        else
            HartWrDelimiter = HART_D_STX | HART_D_LONG_FRAME;
// adjust for indirect commands
        if (ptr->cn > 256)
            HartWrCommandByte = 31;
        else
            HartWrCommandByte = ptr->cn;
        WriteHartFrame();                                                                // re-use old settings
        return (0);
    }
    bzero(HartWrDataBuffer, HART_DATABUF_LEN);
// if command 0, forget all previous data about device
    if (ptr->cn == 0)
        ClearTables();
// init the addressbytes to 0 for broadcast
    if ((ptr->cn == 0)
        || (((ptr->cn == 11) || (ptr->cn == 21) || (ptr->cn == 73))
            && (broadcastflag == 1))) {
        HartWrAddressBytes[0] = 0;
        HartWrAddressBytes[1] = 0;
        HartWrAddressBytes[2] = 0;
        HartWrAddressBytes[3] = 0;
        HartWrAddressBytes[4] = 0;
        HartWrNumPreambles = NUM_PREAM;
    } else {
        for (i = 0; i < 5; i++)
// copy the LONG address info
            HartWrAddressBytes[i] = Slaves[polladdress].longaddress[i];
    }
// clear screen downwards
    move(3, 0);
    clrtobot();
// do we have something in database for this command AND devicetype
    HartWrDataByteCount = 0;
    fl = ReadHartRecord(ptr->cn, liptr, 0, DB_MATCH);                                    // re-use fl
// check command, change to 16 bit commands when needed
    if (ptr->cn >= 256) {
        index = 2;
        HartWrDataBuffer[0] = (ptr->cn >> 8);
        HartWrDataBuffer[1] = ptr->cn & 0xff;
        HartWrCommandByte = 31;                                                          // make indirect command
    } else
        HartWrCommandByte = ptr->cn;
// command without parameters
    if (fl == -1) {
        move(ypos + 1, 5);
        printw("No Request- and Response Data, Command Command ?");                      // command command
    } else {
//
// loop to collect user input, parse database record for each item
//
        while (ReadHartRecord(ptr->cn, liptr, parm, DB_REQ_DATA) == 0) {                 // until all items are done
            fc = (int) ((char) liptr->pt[0]);
            fl = liptr->sz;                                                              // byte size in frame
            ixpos = liptr->rx + xpos;                                                    // screen positions
            iypos = liptr->ry + ypos;
            move(liptr->ry + ypos, liptr->rx + xpos);                                    // position
            aptr = HartWrDataBuffer + index;                                             // where Hart frame data is collected
            switch (fc) {
            case 'P':                                                                   // packed ascii
                memset(&mybuf, 0x20, sizeof(mybuf));
                if (GetInput(ixpos, iypos, 32, liptr->tt, formbuf, 0) < 0)
                    return (-3);
                for (i = 0; i < strlen(formbuf); i++)
                    mybuf[i] = formbuf[i];
                for (i = 0; i < fl / 3; i++) {
                    pack((char *) mybuf + (i * 4), (char *) aptr + (i * 3));
                }
                break;
            case 'U':                                                                   // unsigned
                if (GetInput(ixpos, iypos, 10, liptr->tt, mybuf, 0) < 0)
                    return (-3);
                if (strlen(mybuf) == 0) {                                                // empty input
                    escape = 1;                                                          // allow shorter request frames
                    break;
                }
                sscanf(mybuf, "%ld", &uuv.ul);
                for (i = 0; i < fl; i++) {
                    *(aptr + (fl - 1) - i) = uuv.uc[i];
                }
                break;
            case 'D':                                                                   // date
                if (GetInput(ixpos, iypos, 10, liptr->tt, mybuf, 0) < 0)
                    return (3);
                if (strlen(mybuf) == 0) {
                    escape = 1;
                    break;
                }
                if (sscanf(mybuf, "%2d-%2d-%4d", &k, &j, &i) != 3)
                    return (-1);;
                *aptr = (unsigned char) k;
                *(aptr + 1) = (unsigned char) j;
                *(aptr + 2) = (unsigned char) (i - 1900);
                break;
            case 'Z':                                                                   // cmd 48 status bytes
                DataCopy(aptr, cmd48data, fl);
                break;
            case 'X':                                                                   // hexadecimal
                if (GetInput(ixpos, iypos, 5, liptr->tt, mybuf, 0) < 0)
                    return (-3);
                if (strlen(mybuf) == 0) {
                    escape = 1;
                    break;
                }
                if (sscanf(mybuf, "%X", (unsigned int *) &uuv.ul) != 1)
                    return (-1);
                if (fl == 1)
                    *aptr = uuv.uc[0];
                if (fl == 2) {
                    *aptr = uuv.uc[1];
                    *(aptr + 1) = uuv.uc[0];
                }
                break;
            case 'F':                                                                   // float
                if (GetInput(ixpos, iypos, 10, liptr->tt, mybuf, 0) < 0)
                    return (-3);
                if (strlen(mybuf) == 0) {
                    escape = 1;
                    break;
                }
                sscanf(mybuf, "%f", &uuv.uf);
                DataCopy((BYTE *) aptr, (BYTE *) cvffl(uuv.uf),
                         sizeof(float));
                break;
            case 'A':                                                                   // plain ascii
                memset(aptr, 0x20, sizeof(mybuf));
                if (GetInput(ixpos, iypos, 32, liptr->tt, formbuf, 0) < 0)
                    return (-3);
                DataCopy((BYTE *) aptr, (BYTE *) & formbuf, fl);
                break;
            case 'V':                                                                   // constant
                *aptr = liptr->rx;
                break;
            }                                                                            // switch
            if (escape)                                                                  // we're done?
                break;
            index += fl;
            parm++;
        }                                                                                // while
    }                                                                                    // else
    if (cmdrepflag)
        repmode = 1;                                                                     // update repeat for next cycle
    else
        repmode = 0;
    HartWrDataByteCount = index;
    if (ptr->cn == 0)
        HartWrDelimiter = HART_D_STX;
    else
        HartWrDelimiter = HART_D_STX | HART_D_LONG_FRAME;
    WriteHartFrame();
    return (0);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ProcessRepsonse, process normal response frames
//
int ProcessResponse()
{
    HT localinfo, priminfo;
    HT *liptr = &localinfo;
    HT *ptr = &priminfo;
    unsigned char *aptr;
    char mybuf[1024], formbuf[128], parmcopy[128];
    int xpos = 0, ypos = 8, hartcnt, localcommand = 0;
    int parm = 1, i, j, mytl = 0;
    int fc = 0, fl = 0, index = 0, start = READ_DATA_OFFSET;
    unsigned long mylong;
	long myslong;

    hartcnt = HartDataByteCount;
// if indirect command, get real command code
    if (HartCommandByte == 31) {
        localcommand = (HartDataBuffer[2] << 8) + HartDataBuffer[3];
        start = READ_DATA_OFFSET + IND_CMD_OFFSET;
        hartcnt -= 2;
    } else {
        localcommand = HartCommandByte;
    }
// lookup database for this repsonse frame
    if (ReadHartRecord(localcommand, ptr, 0, DB_MATCH) != 0) {
        move(4, 0);
        clrtobot();
        for(i = 0; i <= HartDataByteCount; i++){
            printw("%02x ", (unsigned char)HartDataBuffer[1 + i]);
            move(4, i * 3);
        }
        move(25, 0);
        printw("Could not access record for command:%d devtype:%04x", localcommand, h_devtype);
        cmdrepflag = 0;
        return (-1);
    }
//
    aptr = HartDataBuffer + start;
    hartcnt -= 2;
// for commands that return basic info, save it
    if ((ptr->cn == 0) || (ptr->cn == 11) || (ptr->cn == 21) || (ptr->cn == 73)) {       // save device address
        polladdress = HartAddressBytes[0] & MAXPOLL;
        h_devtype = *(aptr + 2) + (*(aptr + 1) << 8);
        Slaves[polladdress].longaddress[0] = *(aptr + 1) & 0x3f;
        Slaves[polladdress].longaddress[1] = *(aptr + 2);
        Slaves[polladdress].longaddress[2] = *(aptr + 9);
        Slaves[polladdress].longaddress[3] = *(aptr + 10);
        Slaves[polladdress].longaddress[4] = *(aptr + 11);
        ptr->dt = h_devtype;
        HartWrNumPreambles = (*(aptr + 3) < 5) ? 5 : *(aptr + 3);
    }
//
    PrHead(ptr);                                                                         // last used line 5
    mybuf[0] = '\0';
//
    if (ptr->cn == 48)                                                                   // save data field for next cmd 48
        DataCopy(cmd48data, aptr, sizeof(cmd48data));
//
    if (ReadHartRecord(ptr->cn, liptr, 0, DB_RSP_DATA) == 0) {
// commands with no response data defined, show data bytes
        if (hartcnt){
            xpos = 5;
            for (j = 0; j < hartcnt;) {
                for (i = 0; i < 16; i++) {
                    move(ypos, xpos + i * 3);
                    printw("%02x ", *(aptr + j));
                    j++;
                }
                ypos++;
            }
        }
        refresh();
        return (0);
    } else {
// has response data
        move(ypos, xpos);
        clrtobot();
//
// parse the database record for each defined field
//
        while ((ReadHartRecord(ptr->cn, liptr, parm, DB_RSP_DATA) == 0)
               && (index  < hartcnt)) {
            mytl = (strlen(liptr->tt) > 8) ? 1 : 0;									// title length
            mylong = 0;
            myslong = 0;
            fc = (int) ((char) liptr->pt[0]);
            fl = liptr->sz;
            move(liptr->ry + ypos, liptr->rx + xpos);
            aptr = HartDataBuffer + start + index;
            switch (fc) {
            case 'I':                                                                   // unsigned
                for (i = 0; i < fl; i++) {
                    myslong = myslong << 8;
                    myslong += *(unsigned char *) (aptr + i);
                }
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%ld", liptr->tt,
                            (long) myslong);
                else
                    sprintf((char *) &mybuf, "%8s :%ld", liptr->tt,
                            (long) myslong);
                break;
            case 'U':                                                                   // unsigned
                for (i = 0; i < fl; i++) {
                    mylong = mylong << 8;
                    mylong += *(unsigned char *) (aptr + i);
                }
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%lu", liptr->tt,
                            (unsigned long) mylong);
                else
                    sprintf((char *) &mybuf, "%8s :%lu", liptr->tt,
                            (unsigned long) mylong);
                break;
            case 'X':                                                                   // hexadecimal
                for (i = 0; i < fl; i++) {
                    mylong = mylong << 8;
                    mylong += *(unsigned char *) (aptr + i);
                }
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%02lx", liptr->tt,
                            mylong);
                else
                    sprintf((char *) &mybuf, "%8s :%02lx", liptr->tt, mylong);
                break;
            case 'L':                                                                   // LONG
                for (i = 0; i < fl; i++) {
                    mylong = mylong << 8;
                    mylong += *(unsigned char *) (aptr + i);
                }
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%ld", liptr->tt, mylong);
                else
                    sprintf((char *) &mybuf, "%8s :%ld", liptr->tt, mylong);
                break;
            case 'F':                                                                   // float
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%.2f", liptr->tt,
                            cvtfl(aptr));
                else
                    sprintf((char *) &mybuf, "%8s :%.2f", liptr->tt,
                            cvtfl(aptr));
                break;
            case 'P':                                                                   // packed ascii
                for (i = 0; i < fl / 3; i++) {
                    unpack((char *) aptr + (i * 3),
                           (char *) formbuf + (i * 4));
                }
                formbuf[i * 4] = '\0';
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%s", liptr->tt, formbuf);
                else
                    sprintf((char *) &mybuf, "%8s :%s", liptr->tt, formbuf);
                break;
            case 'D':                                                                   // date field
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%02d-%02d-%04d",
                            liptr->tt, *aptr, *(aptr + 1),
                            *(aptr + 2) + 1900);
                else
                    sprintf((char *) &mybuf, "%8s :%02d-%02d-%04d",
                            liptr->tt, *aptr, *(aptr + 1),
                            *(aptr + 2) + 1900);
                break;
            case 'A':                                                                   // plain ascii
                strncpy((char *) &parmcopy, (char *) aptr, fl);
                parmcopy[fl] = '\0';
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%s", liptr->tt,
                            (char *) &parmcopy[0]);
                else
                    sprintf((char *) &mybuf, "%8s :%s", liptr->tt,
                            (char *) &parmcopy[0]);
                break;
            }
            printw("%s", mybuf);
            refresh();
            parm++;
            index += fl;
        }
        aptr = HartDataBuffer + start + index;
        *aptr = '\0';
    }
    return (0);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ProcessBrResponse,  process Burst response frames
//
int ProcessBrResponse()
{
    HT localinfo, priminfo;
    HT *liptr = &localinfo;
    HT *ptr = &priminfo;
    unsigned char *aptr;
    char mybuf[1024], formbuf[128], parmcopy[128];
    int xpos = 0, ypos = 5, hartcnt, localcommand = 0;
    int parm = 1, i, j, mytl = 0;
    int fc = 0, fl = 0, index = READ_DATA_OFFSET;
    unsigned long mylong;

    hartcnt = HartDataByteCount;
// if indirect command, get real command code
    if (HartBrCommandByte == 31) {
        localcommand = (HartBrDataBuffer[2] << 8) + HartBrDataBuffer[3];
        index += 2;
        hartcnt -= 2;
    } else {
        localcommand = HartBrCommandByte;
    }
//
    if (ReadHartRecord(localcommand, ptr, 0, DB_MATCH | DB_BURSTDEV) != 0) {
        move(4, 0);
        clrtobot();
        move(10, 0);
        printw("Could not access record for command!");
        cmdrepflag = 0;
        return (-1);
    }
//
    PrBrHead(ptr);                                                                       // last used line = 5
    mybuf[0] = '\0';

    aptr = HartBrDataBuffer + index;
    hartcnt -= 2;
//
    b_devtype = ((HartBrAddressBytes[0] & 0x7f) << 8) + HartBrAddressBytes[1];
    ptr->dt = b_devtype;

    if (ReadHartRecord(ptr->cn, liptr, 0, DB_RSP_DATA) == 0) {
// commands with no response data
        if (hartcnt) {                                                                   // if databytes, show them
            xpos = 5;
            for (j = 0; j < hartcnt;) {
                for (i = 0; i < 16; i++) {
                    move(ypos, xpos + i * 3);
                    printw("%02x ", *(aptr + j));
                    j++;
                }
                ypos++;
            }
        }
        refresh();
        return (0);
    } else {
// has response data
        move(ypos, xpos);
        clrtobot();
        if (ptr->cn == 31) {
            index += 2;                                                                  // skip over indirect command nr
            ptr->cn = (*(aptr) << 8) + *(aptr + 1);
        }
//
// parse the database record for each defined field
//
        while ((ReadHartRecord
                (ptr->cn, liptr, parm, DB_RSP_DATA | DB_BURSTDEV) == 0)
               && ((index - READ_DATA_OFFSET) < hartcnt)) {
            mytl = (strlen(liptr->tt) > 8) ? 1 : 0;
            mylong = 0;
            fc = (int) ((char) liptr->pt[0]);
            fl = liptr->sz;
            move(liptr->ry + ypos, liptr->rx + xpos);
            aptr = HartBrDataBuffer + index;
            switch (fc) {
            case 'I':                                                                   // unsigned
                for (i = 0; i < fl; i++) {
                    mylong = mylong << 8;
                    mylong += *(unsigned char *) (aptr + i);
                }
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%ld", liptr->tt,
                            (long) mylong);
                else
                    sprintf((char *) &mybuf, "%8s :%ld", liptr->tt,
                            (long) mylong);
                break;
            case 'U':
                for (i = 0; i < fl; i++) {
                    mylong = mylong << 8;
                    mylong += *(unsigned char *) (aptr + i);
                }
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%ld", liptr->tt,
                            (unsigned long) mylong);
                else
                    sprintf((char *) &mybuf, "%8s :%ld", liptr->tt,
                            (unsigned long) mylong);
                break;
            case 'X':
                for (i = 0; i < fl; i++) {
                    mylong = mylong << 8;
                    mylong += *(unsigned char *) (aptr + i);
                }
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%02lx", liptr->tt,
                            mylong);
                else
                    sprintf((char *) &mybuf, "%8s :%02lx", liptr->tt, mylong);
                break;
            case 'L':
                for (i = 0; i < fl; i++) {
                    mylong = mylong << 8;
                    mylong += *(unsigned char *) (aptr + i);
                    if ((fl == 3) && (i == 0))
                        mylong &= 0xffffff3fL;
                }
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%ld", liptr->tt, mylong);
                else
                    sprintf((char *) &mybuf, "%8s :%ld", liptr->tt, mylong);
                break;
            case 'F':
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%.2f", liptr->tt,
                            cvtfl(aptr));
                else
                    sprintf((char *) &mybuf, "%8s :%.2f", liptr->tt,
                            cvtfl(aptr));
                break;
            case 'P':
                for (i = 0; i < fl / 3; i++) {
                    unpack((char *) aptr + (i * 3),
                           (char *) formbuf + (i * 4));
                }
                formbuf[i * 4] = '\0';
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%s", liptr->tt, formbuf);
                else
                    sprintf((char *) &mybuf, "%8s :%s", liptr->tt, formbuf);
                break;
            case 'D':
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%02d-%02d-%04d",
                            liptr->tt, *aptr, *(aptr + 1),
                            *(aptr + 2) + 1900);
                else
                    sprintf((char *) &mybuf, "%8s :%02d-%02d-%04d",
                            liptr->tt, *aptr, *(aptr + 1),
                            *(aptr + 2) + 1900);
                break;
            case 'A':
                strncpy((char *) &parmcopy, (char *) aptr, fl);
                parmcopy[fl] = '\0';
                if (mytl)
                    sprintf((char *) &mybuf, "%16s :%s", liptr->tt,
                            (char *) &parmcopy[0]);
                else
                    sprintf((char *) &mybuf, "%8s :%s", liptr->tt,
                            (char *) &parmcopy[0]);
                break;
            }
            printw("%s", mybuf);
            parm++;
            index += fl;
        }
    }
    return (0);
}

//----------------------------------------------------------------------
//
// ClearTables, erase command 48 data info
//
void ClearTables()
{
    bzero(cmd48data, sizeof(cmd48data));
}

//----------------------------------------------------------------------
//
// DataCopy, compact
//
unsigned char *DataCopy(unsigned char *Dest, const unsigned char *Src,
                        int Len)
{
    while (Len) {
        Len--;
        *Dest++ = *Src++;
    }
    return (Dest);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Read  Hart Request Record, consult data base for given command
//
int ReadHartRecord(int commandno, HT * htptr, int parm, int flag)
{
#ifdef HAS_DATABASE

    MYSQL_RES *res_set;
    MYSQL_ROW row;
    char locbuf[1024], *errstr;
#endif
    int result = -1;
    unsigned short ldevtype;

    if ((commandno < 122) || ((commandno >= 512) && (commandno <64768)))
        ldevtype = 0;
    else {
        if (flag & DB_BURSTDEV)
            ldevtype = b_devtype;
        else
            ldevtype = h_devtype;
    }

    if(!tf_opt)
    {
#ifdef HAS_DATABASE
      
	switch (flag & 0x0f) {
	case DB_MATCH:                                                                      // search general
	    snprintf(locbuf, 1023,
		    "SELECT * FROM %s WHERE h_commandno=%d AND h_devtype=%d",
		    (char *) &db_table, commandno, ldevtype);
	    break;
	case DB_REQ_DATA:                                                                   // search request info
    //                 "SELECT * FROM %s WHERE h_commandno=%d AND h_devtype=%d AND h_requestparm=%d AND h_responseparm=0 AND h_transaction=%d",
	    snprintf(locbuf, 1023,
		    "SELECT * FROM %s WHERE h_commandno=%d AND h_devtype=%d AND h_requestparm=%d AND h_responseparm=0",
		    (char *) &db_table, commandno, ldevtype, parm);
    //                 (char *) &db_table, commandno, ldevtype, parm, transaction);
	    break;
	case DB_RSP_DATA:                                                                   // search response info
    //                 "SELECT * FROM %s WHERE h_commandno=%d AND h_devtype=%d AND h_responseparm=%d AND h_requestparm=0 AND h_transaction=%d ",
	    snprintf(locbuf, 1023,
		    "SELECT * FROM %s WHERE h_commandno=%d AND h_devtype=%d AND h_responseparm=%d AND h_requestparm=0 ",
		    (char *) &db_table, commandno, ldevtype, parm);
    //                 (char *) &db_table, commandno, ldevtype, parm, transaction);
	    break;
	}
	if(mysql_query(db_conn, locbuf) != 0)
	{
	    errstr = (char*) mysql_error(db_conn);
	    if(errstr[0] != '\0')
	    {
		fprintf(stderr, "mysql error %s\n", errstr);
		return (result);
	    }
	}
	res_set = mysql_store_result(db_conn);
	errstr = (char*) mysql_error(db_conn);
	if(errstr[0] != '\0')
	{
	    fprintf(stderr, "mysql error %s\n", errstr);
	    mysql_free_result(res_set);
	    return (result);
	}
    //        return (-1);
	row = mysql_fetch_row(res_set);
	if (row) {
	    htptr->dt = atoi(row[0]);                                                        // h_devtype
	    htptr->cn = atoi(row[1]);                                                        // h_commandno
	    htptr->rq = atoi(row[2]);                                                        // h_requestparm
	    htptr->rs = atoi(row[3]);                                                        // h_responseparm
	    htptr->sz = atoi(row[4]);                                                        // h_parmsize
	    strcpy((char *) htptr->pt, row[5]);                                              // h_parmtype
	    htptr->rx = atoi(row[6]);                                                        // h_relx
	    strcpy((char *) htptr->tt, row[7]);                                              // h_title
	    htptr->ry = atoi(row[8]);                                                        // h_rely
	    htptr->tn = atoi(row[9]);                                                        // h_transaction
	    result = 0;
	}
	mysql_free_result(res_set);

#endif      
    }
//
// use table to get hart frame details
//
    else
    {
#ifdef HAS_TABLEFILE
		switch (flag & 0x0f) {
		case DB_MATCH:                                                                      // search general
			result = ParseTableFile(commandno, ldevtype,  htptr);
			break;

		case DB_REQ_DATA:                                                                   // search request info
			result = ParseTableReqFile(commandno, ldevtype, parm, htptr);
			break;

		case DB_RSP_DATA:                                                                   // search response info
			result = ParseTableRspFile(commandno, ldevtype, parm, htptr);
			break;
   	 	}

#endif      
    }
    return (result);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ParseTableFile, find mathing record
//
int ParseTableFile(int cmd, int devtype, HT *htp)
{
	HT myhtp;
	int result;

	scanlinecount = 0;

	fseek(tf_file, 0L, 0);
// check if file contains lines
	if(htp == NULL)
	{
		while (1)
		{
			result =ParseTableLine(&myhtp);
			if(result == 0)
				continue;
			if(result < 0)
				return(-1);
			return(0);
		}
	}

	while(ParseTableLine( &myhtp) == 0)
	{
		if(devtype != myhtp.dt)
			continue;
		if(cmd != myhtp.cn)
			continue;
		memcpy((void*)htp, (void*)&myhtp, sizeof(HT));
		return (0);
	}
	return(-1);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ParseTableReqFile, find matching request record
//
int ParseTableReqFile(int cmd, int devtype, int parm, HT *htp)
{
	HT myhtp;

	scanlinecount = 0;

	fseek(tf_file, 0L, 0);

	while(ParseTableLine(&myhtp) == 0)
	{
		if(devtype != myhtp.dt)
			continue;
		if(cmd != myhtp.cn)
			continue;
		if(parm != myhtp.rq)
			continue;
		if(myhtp.rs != 0)
			continue;
		memcpy((void*)htp, (void*)&myhtp, sizeof(HT));

		return(0);

	}
	return(-1);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ParseTableRspFile, find matching respone record
//
int ParseTableRspFile(int cmd, int devtype, int parm, HT *htp)
{
	HT myhtp;

	scanlinecount = 0;

	fseek(tf_file, 0L, 0);

	while(ParseTableLine(&myhtp) ==  0)
	{
		if(devtype != myhtp.dt)
			continue;
		if(cmd != myhtp.cn)
			continue;
		if(parm != myhtp.rs)
			continue;
		if(myhtp.rq != 0)
			continue;
		memcpy((void*)htp, (void*)&myhtp, sizeof(HT));
		return(0);
	}
	return(-1);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ParseTableLine, parse parameter table line by line
// return results in htp
//
int ParseTableLine(HT *htp)
{
	char mybuf[512];
	char *myptr = (char*)&mybuf;
	char *myindex;
	int temp = 0;

	do {
		scanlinecount++;
		if(fgets(myptr, 510, tf_file) == NULL)
			return (1);
	} while (*myptr == '#');	

	if((myindex = index(myptr, ',')) == NULL)
		return (-1);
	if(sscanf(myptr, "%d", &temp) != 1)
		return (-1);
	htp->dt = (unsigned short) temp;
	myptr = myindex + 1;
//
	if((myindex = index(myptr, ',')) == NULL)
		return (-1);
	if(sscanf(myptr, "%d", &temp) != 1)
		return (-1);
	htp->cn = (unsigned short) temp;
	myptr = myindex + 1;
//
	if((myindex = index(myptr, ',')) == NULL)
		return (-1);
	if(sscanf(myptr, "%d", &temp) != 1)
		return (-1);
	htp->rq = (char) temp;
	myptr = myindex + 1;
//
	if((myindex = index(myptr, ',')) == NULL)
		return (-1);
	if(sscanf(myptr, "%d", &temp) != 1)
		return (-1);
	htp->rs = (char) temp;
	myptr = myindex + 1;
//
	if((myindex = index(myptr, ',')) == NULL)
		return (-1);
	if(sscanf(myptr, "%d", &temp) != 1)
		return (-1);
	htp->sz = (char) temp;
	myptr = myindex + 1;
//
	if((myindex = index(myptr, ',')) == NULL)
		return (-1);
	if(sscanf(myptr, "%1s", htp->pt) != 1)
		return (-1);
	myptr = myindex + 1;
//
	if((myindex = index(myptr, ',')) == NULL)
		return (-1);
	if(sscanf(myptr, "%d", &temp) != 1)
		return (-1);
	htp->rx = (char) temp;
	myptr = myindex + 1;
//
	if((myindex = index(myptr, ',')) == NULL)
		return (-1);
	if(sscanf(myptr, "%[^,]s", htp->tt) != 1)
		return (-1);
	if(strlen(htp->tt) != 16)
		return(-1);
	myptr = myindex + 1;
//
	if((myindex = index(myptr, ',')) == NULL)
		return (-1);
	if(sscanf(myptr, "%d", &temp) != 1)
		return (-1);
	htp->ry = (char) temp;
	myptr = myindex + 1;
//
	if(sscanf(myptr, "%d", &temp) != 1)
		return (-1);
	htp->tn = (char) temp;
	myptr = myindex;

	return(0);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// conversion helpers
//
// nw2float
float cvtfl(unsigned char *ptr)
{
    union
    {
        float uf;
        char uc[4];
    } uu;
    uu.uc[0] = *(ptr + 3);
    uu.uc[1] = *(ptr + 2);
    uu.uc[2] = *(ptr + 1);
    uu.uc[3] = *ptr;
    return (uu.uf);
}

// float2nw
char *cvffl(float fl)
{
    static unsigned char buf[4];
    union
    {
        float uf;
        unsigned long ul;
    } uu;
    uu.uf = fl;
    buf[0] = (uu.ul >> 24) & 0xff;
    buf[1] = (uu.ul >> 16) & 0xff;
    buf[2] = (uu.ul >> 8) & 0xff;
    buf[3] = (uu.ul) & 0xff;
    return ((char *) &buf);
}

void unpack(char *ptr, char *txt)
{
    char *t = txt;
    unsigned char a, b, c, d;
    a = *(ptr);
    c = b = *(ptr + 1);
    d = *(ptr + 2);
    a = a >> 2;
    a |= (a & 0x20 ? 0 : 0x40);
    *t++ = a;
    a = *(ptr);
    a &= 0x03;
    a = a << 4;
    b = b >> 4;
    b |= a;
    b |= (b & 0x20 ? 0 : 0x40);
    *t++ = b;
    c &= 0x0f;
    c = c << 2;
    a = d >> 6;
    ;
    a &= 0x03;
    c |= a;
    c |= (c & 0x20 ? 0 : 0x40);
    *t++ = c;
    d &= 0x3f;
    d |= (d & 0x20 ? 0 : 0x40);
    *t++ = d;
    *t = '\0';
}

void pack(char *ptr, char *txt)
{
    int i = 0;
    char *t = txt;
    for (i = 0; i < strlen(ptr); i++) {
        *(ptr + i) = toupper(*(ptr + i));
    }
    unsigned char a, b, c, d;
    a = *(ptr);
    a &= 0x3f;
    a = a << 2;
    b = *(ptr + 1);
    b &= 0x30;
    b = b >> 4;
    a |= b;
    *t++ = a;
    b = *(ptr + 1);
    b &= 0x0f;
    b = b << 4;
    d = c = *(ptr + 2);
    c &= 0x3c;
    c = c >> 2;
    b |= c;
    *t++ = b;
    b = *(ptr + 3);
    b &= 0x3f;
    d &= 0x03;
    d = d << 6;
    b |= d;
    *t++ = b;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// PrBrHead, print burst mode header on screen
//
void PrBrHead(HT * ptr)
{
    unsigned long ll;
    char statbuf[] = "-------- [M]";

    if (HartBrDataBuffer[1] & 128)
        statbuf[0] = 'M';
    if (HartBrDataBuffer[1] & 64)
        statbuf[1] = 'C';
    if (HartBrDataBuffer[1] & 32)
        statbuf[2] = 'R';
    if (HartBrDataBuffer[1] & 16)
        statbuf[3] = 'E';
    if (HartBrDataBuffer[1] & 8)
        statbuf[4] = 'F';
    if (HartBrDataBuffer[1] & 4)
        statbuf[5] = 'S';
    if (HartBrDataBuffer[1] & 2)
        statbuf[6] = 'O';
    if (HartBrDataBuffer[1] & 1)
        statbuf[7] = 'P';

    move(2, 2);
    printw("Delimiter:%3x   ", HartDelimiter);
    move(2, 17);
    if (HartDelimiter & 0x80)
        printw("Address: %02x %02x %02x %02x %02x",
               HartBrAddressBytes[0], HartBrAddressBytes[1],
               HartBrAddressBytes[2], HartBrAddressBytes[3],
               HartBrAddressBytes[4]);
    else
        printw("Address: %02x                    ", HartBrAddressBytes[0]);
    move(2, 46);
    ll = (HartBrAddressBytes[2] << 16) + (HartBrAddressBytes[3] << 8) +
        HartBrAddressBytes[4];
    printw("Unit:%09d ", ll);
    move(2, 68);
    printw("Device:%04x ", b_devtype);
    move(3, 2);
    printw
        ("Command:%5d  Datacount:%3d   Status1:%3d  Status2:%s  Checksum:%02x",
         ptr->cn, HartDataByteCount, HartBrDataBuffer[0], &statbuf,
         HartCheckSum);
}

//----------------------------------------------------------------------
//
// PrHead, print frame header info
//
void PrHead(HT * ptr)
{
    unsigned long ll;
    char statbuf[] = "-------- [N]";

    if (HartDataBuffer[1] & 128)
        statbuf[0] = 'M';
    if (HartDataBuffer[1] & 64)
        statbuf[1] = 'C';
    if (HartDataBuffer[1] & 32)
        statbuf[2] = 'R';
    if (HartDataBuffer[1] & 16)
        statbuf[3] = 'E';
    if (HartDataBuffer[1] & 8)
        statbuf[4] = 'F';
    if (HartDataBuffer[1] & 4)
        statbuf[5] = 'S';
    if (HartDataBuffer[1] & 2)
        statbuf[6] = 'O';
    if (HartDataBuffer[1] & 1)
        statbuf[7] = 'P';
    if (BurstModeActive)
        statbuf[10] = 'B';

    move(3, 2);
    printw("Delimiter:%3x   ", HartDelimiter);
    move(3, 17);
    if (HartDelimiter & 0x80)
        printw("Address: %02x %02x %02x %02x %02x",
               HartAddressBytes[0], HartAddressBytes[1], HartAddressBytes[2],
               HartAddressBytes[3], HartAddressBytes[4]);
    else
        printw("Address: %02x                    ", HartAddressBytes[0]);
    move(3, 46);
    ll = (HartAddressBytes[2] << 16) + (HartAddressBytes[3] << 8) +
        HartAddressBytes[4];
    printw("Unit:%09d ", ll);
    move(3, 68);
    printw("Device:%04x ", h_devtype);
    move(4, 2);
    printw
        ("Command:%5d  Datacount:%3d   Status1:%3d  Status2:%s  Checksum:%02x",
         ptr->cn, HartDataByteCount, HartDataBuffer[0], &statbuf,
         HartCheckSum);
}
