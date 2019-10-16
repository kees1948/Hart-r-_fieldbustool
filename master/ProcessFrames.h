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
#ifndef __PROCESSFRAMES_H__
#define __PROCESSFRAMES_H__


int ProcessRequest(HT *);
int ProcessResponse(void);
int ProcessBrResponse(void);
void ClearTables(void);
unsigned char *DataCopy(unsigned char *, const unsigned char *, int);
int ReadHartRecord(int, HT *, int, int);
float cvtfl(unsigned char *);
char *cvffl(float);
void unpack(char *, char *);
void pack(char *, char *);
void PrBrHead(HT *);
void PrHead(HT *);
int	ParseTableFile(int, int, HT *);
int	ParseTableReqFile(int, int, int, HT *);
int	ParseTableRspFile(int, int, int, HT *);
int ParseTableLine(HT*);

#endif // __PROCESSFRAMES_H__
