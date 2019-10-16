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
#ifndef		__DATALINK_H__
#define		__DATALINK_H__

#define	DL_ENABLED		1
#define DL_USING		2
#define	DL_WATCHING		4


void *ReadHartFrame(void *);
void CheckDump(int, int);
int ReadHartByte(void);
void WriteHartFrame(void);
void WriteHartByte(unsigned char);
void SetErrorBits(BYTE);

int ReadHartByte(void);

void ReceiveState0(void), ReceiveState1(void), ReceiveState2(void),
ReceiveState3(void);
void ReceiveState4(void), ReceiveState5(void), ReceiveState6(void),
ReceiveState7(void);
void ReceiveState8(void);
void XMitState0(void), XMitState1(void), XMitState2(void), XMitState3(void);
void XMitState4(void), XMitState5(void), XMitState6(void), XMitState7(void);
void XMitState8(void), XMitState9(void), XMitState10(void);
void CarrierOn(void), CarrierOff(void);
void CheckBurstMode(void);
void CopyBurstAddress(void);

#endif // __DATALINK_H__
