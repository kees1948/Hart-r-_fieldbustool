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

/*
 *
 * This is the BUS Engine, it keeps synchronised with the Hart bus
 * It maintains a state-machine for this
 *
 *
 *
 *
 *
 */

#include    "Hart.h"
#include 	"Main.h"
#include    "DataLink.h"
#include    "ProcessFrames.h"



extern int parportfd;
extern struct pollfd hartfd;

extern BYTE WhoAmI;
extern int polladdress;
extern int burstlisten;
extern int datalisten;
extern int fdti_opt;
extern int dump_opt;
extern int hwht_opt;
extern int ana_opt;
extern int timing_opt;
extern int abstim_opt;
extern int cmdstringflag;

extern pthread_mutex_t lt_mutex;
extern pthread_mutex_t cd_mutex;
extern pthread_mutex_t ft_mutex;
extern pthread_mutex_t mr_mutex;

struct  timeval tv;
int Timer10mS = 0;
int writecharactertime = 0;
double dltime = 0.0;
volatile int FrameTimer = 17;
extern volatile int LGTimer;
extern volatile int LQTimer;
extern volatile int CDTimer;
extern volatile int BuMoTimeout;
extern int savey;
extern int savex;
extern volatile int mainreceived;

BYTE HartDelimiter;
BYTE HartAddressBytes[HART_LADDR_LEN];
BYTE BDeviceAddressBytes[HART_LADDR_LEN];
BYTE HartCommandByte;
BYTE HartExpByteCount;
BYTE HartExpansionBytes[HART_EXP_LEN];
BYTE HartDataByteCount;
BYTE HartDataBuffer[HART_DATABUF_LEN];
BYTE HartResponseByte1;
BYTE HartCheckSum;
volatile int HartSerialState = 0;
volatile BYTE WhoHasToken = PRIMARY;
BYTE MasterAddressed = 0;
BYTE HartWrNumPreambles = 5;
//
BYTE HartWrDelimiter;
BYTE HartWrAddressBytes[HART_LADDR_LEN];
BYTE HartWrCommandByte;
BYTE HartWrDataByteCount;
BYTE HartWrDataBuffer[HART_DATABUF_LEN];
BYTE HartWrCheckSum;
int HartWrSerialState = 0;
//
BYTE HartBrAddressBytes[HART_LADDR_LEN];
BYTE HartBrCommandByte;
//BYTE HartBrExpByteCount;
BYTE HartBrExpansionBytes[HART_EXP_LEN];
//BYTE HartBrDataByteCount;
BYTE HartBrDataBuffer[HART_DATABUF_LEN];
//BYTE HartBrCheckSum;

//
volatile BYTE BurstModeActive = 0;
volatile BYTE TempResponseByte;
volatile BYTE TempDataByte;
volatile BYTE SerialDataCount;
volatile BYTE SerialWrDataCount;
BYTE HartLastFrame;

//
// states for Receive
//
struct rstate
{
    void *(*StatePtr) (void);
} ReceiveStates[] = {
    {
    (void *) &ReceiveState0}, {
    (void *) &ReceiveState1}, {
    (void *) &ReceiveState2}, {
    (void *) &ReceiveState3}, {
    (void *) &ReceiveState4}, {
    (void *) &ReceiveState5}, {
    (void *) &ReceiveState6}, {
    (void *) &ReceiveState7}, {
    (void *) &ReceiveState8}
};

#define MAX_RECEIVE_STATE   sizeof(ReceiveStates)/sizeof(struct rstate)

//
// states for Transmit
//
struct tstate
{
    void *(*StatePtr) (void);
} XMitStates[] = {
    {
    (void *) &XMitState0}, {
    (void *) &XMitState1}, {
    (void *) &XMitState2}, {
    (void *) &XMitState3}, {
    (void *) &XMitState4}, {
    (void *) &XMitState5}, {
    (void *) &XMitState6}, {
    (void *) &XMitState7}, {
    (void *) &XMitState8}, {
    (void *) &XMitState9}
};

#define MAX_XMIT_STATE  sizeof(XMitStates)/sizeof(struct tstate)

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ReadHartFrame, thread, collect a complete Hart frame
//
void *ReadHartFrame(void *arg)
{
    HartSerialState = 0;

    while (1) {
//      if(poll(&hartfd, 1, 1) <= 0){
            pthread_mutex_lock(&mr_mutex);
            if(mainreceived > 1){
                mainreceived = 0;
                ReceiveState0();
            }
            pthread_mutex_unlock(&mr_mutex);
//          us_sleep( 1000);
//      } else {
            read(hartfd.fd, (BYTE*)&TempDataByte, 1);
            pthread_mutex_lock(&ft_mutex);
            FrameTimer = HART_GAP_TIME;
            pthread_mutex_unlock(&ft_mutex);
            TempResponseByte = 0;                                           // left over from slave dll
            if (HartSerialState > MAX_RECEIVE_STATE)
                ReceiveState0();
            ReceiveStates[HartSerialState].StatePtr();
//      }
    }
}

//----------------------------------------------------------------------
//
// ReceiveState0, init essential parameters
//
void ReceiveState0()
{
    SerialDataCount = 0;
    HartResponseByte1 = 0;
    HartCheckSum = 0;
    HartSerialState = 1;
    return;
}

//----------------------------------------------------------------------
//
// ReceiveState1, collect preambles while waiting for delimiter
void ReceiveState1()
{
    if (TempResponseByte) {
		TempResponseByte = 0;
        SerialDataCount = 0;
        return;
    }
// if preambles, count them
    if (TempDataByte == HART_PREAMBLE) {
        CheckDump(0, 0);
        SerialDataCount++;
        return;
    }
// check if delimiter
    switch (TempDataByte & 0x87) {                                                       // mask off expansion bits
    case HART_D_ACK:
    case HART_D_STX:
    case (HART_D_LONG_FRAME | HART_D_ACK):
    case (HART_D_LONG_FRAME | HART_D_STX):
    case (HART_D_LONG_FRAME | HART_D_BACK):
        HartDelimiter = TempDataByte;
        break;
    default:
        SerialDataCount = 0;                                                             // restart search
        return;
    }
// valid preambles count
    if (SerialDataCount < 2) {
        SerialDataCount = 0;
        return;
    }
// is it normal frame or burst frame?
    CheckDump(1, 0);
    HartCheckSum = TempDataByte;
    HartExpByteCount = (TempDataByte & HART_EXP_MASK) >> 5;
    if ((HartDelimiter & HART_DEL_MASK) != HART_D_BACK) {                                //ACK + STX
        bzero(&HartAddressBytes[0], HART_LADDR_LEN);
    } else {                                                                             // BACK
        bzero(&HartBrAddressBytes[0], HART_LADDR_LEN);
    }
    pthread_mutex_lock(&lt_mutex);
    if (WhoAmI == PRIMARY)                                                              // based on longest possible frame
        LQTimer = RT1_TIMEP * 7;
    else
        LQTimer = RT1_TIMES * 7;
        LGTimer = -1;
    pthread_mutex_unlock(&lt_mutex);

    SerialDataCount = 0;
    HartSerialState = 2;
}

//----------------------------------------------------------------------
//
// ReceiveState2, read address bytes
//
void ReceiveState2()
{
    CheckDump(2, 0);
    if (TempResponseByte) {
        ReceiveState8();
        return;
    }
//
    HartCheckSum ^= TempDataByte;
    if ((HartDelimiter & HART_DEL_MASK) != HART_D_BACK) {                                // ACK and STX frame data
// STX or ACK frame
        *(((BYTE *) & HartAddressBytes + SerialDataCount)) = TempDataByte;
// collect burst frame data
    } else {                                                                             // collect burst frame data
        *(((BYTE *) & HartBrAddressBytes + SerialDataCount)) = TempDataByte;
    }
    if(!SerialDataCount){
        if(TempDataByte & 0x80)
            MasterAddressed = PRIMARY;
        else
            MasterAddressed = SECUNDARY;
//      if(TempDataByte & 0x40)
//          BurstModeActive |= 2;
    }
    if (HartDelimiter & HART_D_LONG_FRAME) {                                         // collect long address
        SerialDataCount++;
        if (SerialDataCount < HART_LADDR_LEN)
            return;
    }
    SerialDataCount = 0;
    if (HartExpByteCount)
        HartSerialState = 3;
    else
        HartSerialState = 4;
}

//----------------------------------------------------------------------
//
// ReceiveState3, handle expansion bytes
//
void ReceiveState3()
{
    CheckDump(3, 0);
    if (TempResponseByte) {
        ReceiveState8();
        return;
    }
    HartCheckSum ^= TempDataByte;
    *(((BYTE *) & HartExpansionBytes + SerialDataCount)) = TempDataByte;
    SerialDataCount++;
    if (SerialDataCount < HartExpByteCount)
        return;
    HartSerialState = 4;
}

//----------------------------------------------------------------------
//
// ReceiveState4, commandbyte
//
void ReceiveState4()
{
    CheckDump(4, 0);
    if (TempResponseByte)
        SetErrorBits(TempResponseByte);
    HartCheckSum ^= TempDataByte;
    if ((HartDelimiter & HART_DEL_MASK) != HART_D_BACK) {                                // ACK and STX
        HartCommandByte = TempDataByte;
    } else {
        HartBrCommandByte = TempDataByte;
    }
    HartSerialState = 5;
}

//----------------------------------------------------------------------
//
// ReceiveState5, read databyte count
//
void ReceiveState5()
{
    CheckDump(5, 0);
    if (TempResponseByte) {
        ReceiveState8();
        return;
    }
    HartCheckSum ^= TempDataByte;
    HartDataByteCount = TempDataByte;
    if (HartDataByteCount < 2)                                                       // read error or frame error
        ReceiveState8();
    SerialDataCount = 0;
    HartSerialState = 6;
}

//----------------------------------------------------------------------
//
// ReceiveState6, responsebytes and databytes
//
void ReceiveState6()
{
    CheckDump(6, 0);
    if (TempResponseByte)
        SetErrorBits(TempResponseByte);
    HartCheckSum ^= TempDataByte;
    if ((HartDelimiter & HART_DEL_MASK) != HART_D_BACK) {                                // ACK and STX
        if (SerialDataCount < HART_DATABUF_LEN)
            *(((BYTE *) & HartDataBuffer + SerialDataCount)) = TempDataByte;
        else
            HartResponseByte1 |= CODOFL;
    } else {
        if (SerialDataCount < HART_DATABUF_LEN)
            *(((BYTE *) & HartBrDataBuffer + SerialDataCount)) = TempDataByte;
        else
            HartResponseByte1 |= CODOFL;
    }
    SerialDataCount++;
    if (SerialDataCount >= HartDataByteCount)
        HartSerialState = 7;
}

//----------------------------------------------------------------------
//
// ReceiveState7, checksum and wind-up
// restart bus timers
//
void ReceiveState7()
{
    CheckDump(7, 1);
    if (TempResponseByte)
        SetErrorBits(TempResponseByte);
//
    HartCheckSum ^= TempDataByte;

    if(HartCheckSum) {
        pthread_mutex_lock(&lt_mutex);
        if(WhoAmI == PRIMARY)
            LQTimer = RT1_TIMEP * 7;
        else
            LQTimer = RT1_TIMES * 7;
        LGTimer = -1;
        pthread_mutex_unlock(&lt_mutex);
        return (ReceiveState0());
    }
//
    if(HartAddressBytes[0] & 0x40)
        BurstModeActive |= 2;
//
    WhoHasToken = (MasterAddressed == PRIMARY) ? SECUNDARY : PRIMARY;
    switch (HartDelimiter & HART_DEL_MASK) {
//----------------------------------------------------------------------
//
// STX received
//
    case HART_D_STX:                                                    // another master sent STX
//
        pthread_mutex_lock(&lt_mutex);
        if (WhoAmI == PRIMARY)
            LQTimer = RT1_TIMEP * 2;
        else
            LQTimer = RT1_TIMES * 2;
        LGTimer = -1;
        pthread_mutex_unlock(&lt_mutex);
//
// go resync datalink layer
//
        return (ReceiveState0());
//          break;                                      // resync
//----------------------------------------------------------------------
//
// ACK received
//
    case HART_D_ACK:
//      valid frame
        CheckBurstMode();
//
        if (burstlisten)
            return (ReceiveState0());                                   // not interested in these
//
// ACK & Bmode, BURST device keeps token, wait with transmission
//
        if (BurstModeActive) {
            pthread_mutex_lock(&lt_mutex);
            if (WhoAmI == PRIMARY)
                LQTimer = RT1_TIMEP;
            else
                LQTimer = RT1_TIMES;
            LGTimer = -1;
            pthread_mutex_unlock(&lt_mutex);
            break;
        }
//
// ACK && !BMode, token changes to the 'other'master
// wait for him to answer
//
        pthread_mutex_lock(&lt_mutex);
        if (WhoHasToken == WhoAmI) {
            LGTimer = 2;                                                // I have token, may use bus
        } else {                                                        // To Him
            LGTimer = RT2_TIME;                                         // wait gap
        }
        LQTimer = -1 ;                                                   // disable
        pthread_mutex_unlock(&lt_mutex);
        break;
//----------------------------------------------------------------------
//
// BACK received
//
    case HART_D_BACK:
//
        BurstModeActive |= 2;
        CopyBurstAddress();
// when nothing arrives for some time, erase screen
        BuMoTimeout = 8000;                                                              // 284+284+284 chars
// toggle token to 'other' master
//
        if (datalisten)
            return (ReceiveState0());
//
        if (burstlisten)
            break;
//
        if (WhoHasToken == WhoAmI) {                                                     // do I have token now?
//
// I have the token!, I _may_ transmit
//
            pthread_mutex_lock(&lt_mutex);
            LGTimer = 2;                                                                 // take bus
            LQTimer = -1;                                                                // off
            pthread_mutex_unlock(&lt_mutex);
        } else {
//
// He has Token, I should wait
//
            pthread_mutex_lock(&lt_mutex);
            LGTimer = -1;
            if (WhoAmI == PRIMARY) {
                LQTimer = RT1_TIMEP;
            } else {
                LQTimer = RT1_TIMES;
            }
            pthread_mutex_unlock(&lt_mutex);
        }
        break;
    default:
        pthread_mutex_lock(&lt_mutex);
        LQTimer = RT1_TIMEP * 2;
        LGTimer = -1;                                                                 // don't take bus
        pthread_mutex_unlock(&lt_mutex);
        return (ReceiveState0());
    }
// we arrive here only with ack or back frame
    HartSerialState = HART_FRAME_RECVD;
    pthread_mutex_lock(&mr_mutex);
    mainreceived = 1;
    pthread_mutex_unlock(&mr_mutex);
}

//----------------------------------------------------------------------
//
// ReceiveState8, wait for end of frame
//
void ReceiveState8()
{
// lost sync, start over
          pthread_mutex_lock(&ft_mutex);
          FrameTimer = HART_GAP_TIME ;
          pthread_mutex_unlock(&ft_mutex);
    HartSerialState = 8;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// WriteHartFrame, when we have the bus send off frame
//
void WriteHartFrame()
{
    int g = 1, c = 1, q = 1;

    HartWrSerialState = 0;
//
// arbitrate the bus until allowed
//
    do {
        pthread_mutex_lock(&lt_mutex);
        pthread_mutex_lock(&ft_mutex);
        g = LGTimer;
        q = LQTimer;
        c = FrameTimer;
        pthread_mutex_unlock(&ft_mutex);
        pthread_mutex_unlock(&lt_mutex);
        us_sleep(500);
    } while ((g > 0) || (c > 0) || (q > 0));
//
// we HAVE the bus, start writing immediately
//
    while (1) {
        if (HartWrSerialState > MAX_XMIT_STATE)
            HartWrSerialState = MAX_XMIT_STATE;
        XMitStates[HartWrSerialState].StatePtr();
        if (HartWrSerialState == 9)
            return;
    }
}

//----------------------------------------------------------------------
//
// XMitState0, init neccesary variables
//
void XMitState0()
{
    SerialWrDataCount = HartWrNumPreambles;
    CarrierOn();
    HartWrSerialState = 1;
    return (XMitState1());
}

//----------------------------------------------------------------------
//
// XMitState1, write preambles
//
void XMitState1()
{
    int i;
    if(cmdstringflag){
        for(i = 0; i < HartWrDataByteCount; i++)
            WriteHartByte(HartWrDataBuffer[i]);
        HartWrSerialState = 8;
    } else {
        WriteHartByte(HART_PREAMBLE);
        if (SerialWrDataCount) {
            SerialWrDataCount--;
            return;
        }
        HartWrSerialState = 2;
    }
}

//----------------------------------------------------------------------
//
// XMitState2, write frame delimiter
//
void XMitState2()
{
    WriteHartByte(HartWrDelimiter);
    HartWrCheckSum = HartWrDelimiter;
    SerialWrDataCount = 0;
    HartWrSerialState = 3;
}

//----------------------------------------------------------------------
//
// XMitState3, write address bytes
//
void XMitState3()
{
    BYTE MyByte = *((BYTE *) & HartWrAddressBytes + SerialWrDataCount);
    if (!(HartWrDelimiter & HART_D_LONG_FRAME))
        MyByte = polladdress;
    if (!SerialWrDataCount) {                                                            // set Master Address bit
        MyByte &= 0x7f;
        MyByte |= ((WhoAmI == PRIMARY) ? 0x80 : 0);
    }
    WriteHartByte(MyByte);
    HartWrCheckSum ^= MyByte;
    if (!(HartWrDelimiter & HART_D_LONG_FRAME))
        HartWrSerialState = 4;
    else
        SerialWrDataCount++;
    if (SerialWrDataCount >= HART_LADDR_LEN)
        HartWrSerialState = 4;
}

//----------------------------------------------------------------------
//
// XMitState4, write command byte
//
void XMitState4()
{
    WriteHartByte(HartWrCommandByte);
    HartWrCheckSum ^= HartWrCommandByte;
    HartWrSerialState = 5;
}

//----------------------------------------------------------------------
//
// XMitState5, write databyte count
//
void XMitState5()
{
    WriteHartByte(HartWrDataByteCount);
    HartWrCheckSum ^= HartWrDataByteCount;
    SerialWrDataCount = 0;
    HartWrSerialState = (HartWrDataByteCount) ? 6 : 7;
}

//----------------------------------------------------------------------
//
// XMitState6, if databytes, write them
//
void XMitState6()
{
    BYTE MyByte = *((BYTE *) & HartWrDataBuffer[0] + SerialWrDataCount);
    WriteHartByte(MyByte);
    HartWrCheckSum ^= MyByte;
    SerialWrDataCount++;
    if (SerialWrDataCount >= HartWrDataByteCount)
        HartWrSerialState = 7;
}

//----------------------------------------------------------------------
//
// XMitState7, write checksum byte
//
void XMitState7()
{
    WriteHartByte(HartWrCheckSum);
    HartWrSerialState = 8;
}

//----------------------------------------------------------------------
//
// XMitState8, make sure all bytes are output from modem
// Set arbritration timers
//
void XMitState8()
{
//    WriteHartByte(FLUSHB);
    CarrierOff();
    pthread_mutex_lock(&lt_mutex);
    if (WhoAmI == PRIMARY)
        LQTimer = RT1_TIMEP;
    else
        LQTimer = RT1_TIMES;
    LGTimer = -1;
    pthread_mutex_unlock(&lt_mutex);
    HartWrSerialState = 9;
}

//----------------------------------------------------------------------
//
// XMitState9, all done, we will exit from the Write loop
//
void XMitState9()
{
    HartWrSerialState = 9;
}

//----------------------------------------------------------------------
//
// WriteHartByte, write ONE byte to port (buffers)
//
void WriteHartByte(BYTE Hb)
{
    BYTE MyByte = Hb;
    write(hartfd.fd, &MyByte, 1);
    writecharactertime += 9180;
    return;
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
    int parm = 0;

    if(hwht_opt){
        us_sleep(writecharactertime + 2000);
    } else {
        while (!parm) {
            if (fdti_opt)
                us_sleep(10000);
            ioctl(hartfd.fd, TIOCSERGETLSR, &parm);                     // make sure ALL bytes are written
        }
        if (fdti_opt)
            us_sleep(24000);
    }                                                                   // fdti driver has problems with TIOGETLSR
    ioctl(hartfd.fd, TIOCMGET, &parm);
    parm &= ~TIOCM_RTS;                                                 /* RTS OFF */
    ioctl(hartfd.fd, TIOCMSET, &parm);
}


void SetErrorBits(BYTE Bit)
{
    if (Bit & OE)
        HartResponseByte1 |= COOVRN;
    if (Bit & FE)
        HartResponseByte1 |= COFRER;
    if (Bit & PE)
        HartResponseByte1 |= COPARE;
}

//
// keep address received
//
void CheckBurstMode()
{
    BYTE localhaddresbytes[HART_LADDR_LEN];

    DataCopy(&localhaddresbytes[0], &HartAddressBytes[0], HART_LADDR_LEN);
// is it bursting slave
    if (HartAddressBytes[0] & 0x40) {
        localhaddresbytes[0] &= 0x3f;
        DataCopy(&BDeviceAddressBytes[0], &localhaddresbytes[0],
                 HART_LADDR_LEN);
        BurstModeActive |= 1;
    } else {
// if it was bursting in the past
        localhaddresbytes[0] &= 0x3f;
        if (bcmp(&BDeviceAddressBytes[0], &localhaddresbytes[0],
             HART_LADDR_LEN) == 0){
            bzero(&BDeviceAddressBytes[0],HART_LADDR_LEN);
            BurstModeActive &= ~3;
        }
    }
}

void CopyBurstAddress()
{
    DataCopy(&BDeviceAddressBytes[0], &HartBrAddressBytes[0], HART_LADDR_LEN);
    BDeviceAddressBytes[0] &= 0x3f;
}

void CheckDump(int state, int flag)
{
    const char* mark[]={"pa","dl","ab","eb","cb","dc","db","cs"};
    struct timeval tv2;
    long t;
    long long at;
    float myfloat;
    double mytime;

    if(!dump_opt)
        return;
    if(timing_opt){
        gettimeofday(&tv2, NULL);

        t = (tv2.tv_usec - tv.tv_usec);
        at = (long long)((long long)tv2.tv_sec * 1000000LL) + ((long long) tv2.tv_usec) ;

        myfloat = (float) t;
        myfloat = myfloat / 1000.0;
        mytime = (double)   at;
        mytime = mytime /1000;

        if(abstim_opt && (state == 1))
            dltime = mytime;

        if(myfloat < 0.0)
            myfloat += 1000.0;
        switch(flag){
            case 0:     if(ana_opt){
                            bcopy((char*)&tv2, (char*)&tv, sizeof(struct timeval));
                            if(state == 1)
                                printf("%s:  %02x   [%.3f mS][%.3f]\n", mark[state & 7], TempDataByte, myfloat, mytime);
                            else
                                printf("%s:  %02x   [%.3f mS]\n", mark[state & 7], TempDataByte, myfloat);
                        } else
                            printf("%02x ", TempDataByte);
                        break;
            case 1:
                        bcopy((char*)&tv2, (char*)&tv, sizeof(struct timeval));
                        if(ana_opt)
                            printf("%s:  %02x   [%.3f mS]\n\n", mark[state & 7], TempDataByte, myfloat);
                        else {
                            if(abstim_opt)
                                printf("%02x   [%.3f mS]:dl\n", TempDataByte, dltime);
                            else
                                printf("%02x   [%.3f mS]\n", TempDataByte, myfloat);
                        }
                        break;
        }
    } else {
        switch(flag){

            case 0:     printf("%02x ", TempDataByte);
                        break;
            case 1:     printf("%02X\n", TempDataByte);
                        break;
        }
    }
    fflush(stdout);
}


