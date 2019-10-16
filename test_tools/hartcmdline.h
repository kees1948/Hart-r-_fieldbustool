/*
 * 
 * This program is released under GNU Public License Version 3 or later.
 * see gpl.txt for the full licence text or www.gnu.org/licences/#GPL
 *
 * Copyright 2010-2013   CWP Schoenmakers  kslinux@gmail.com
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
 * The program uses a MySQL database in which the Request/Response
 * layout of each command is specified
 * 
 * The program creates three threads, one for the transmission fucntions,
 * one for the receiving functions and one that maintains the timers
 * 
 */
#ifndef __MAIN_H__
#define	__MAIN_H__

int HartOpen(char *);
void CarrierOn(void);
void CarrierOff(void);

void SendString(void);
void ReceiveString(void);
void ReportError(int);

unsigned char *DataCopy(unsigned char *, const unsigned char *, int);

void Usage(void);

int ReadConfig(void);

#endif // __MAIN_H__
