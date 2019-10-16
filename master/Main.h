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
#ifndef __MAIN_H__
#define	__MAIN_H__

int 	HartOpen(char *);
void 	DebugOn1(void), DebugOff1(void);
void 	DebugOn2(void), DebugOff2(void);
void 	*ReadFromHart(void *), *WriteToHart(void *), *Timer(void *);
void 	controlc(void), p_end(void);
int 	HartIdentify(int), HartReadFrom(int, int);
void 	setdebug(char*);

int 	check_hdbf(char *);
int 	mdbf_con(char *);
int 	myresponse(char *, char *, unsigned short);
void 	format(int, int, char *, char *);
char 	*dobyte(int, int, char *, char *);
char 	*doint(int, int, char *, char *);
char 	*dou24(int, int, char *, char *);
char 	*dofloat(int, int, char *, char *);

int 	us_sleep(int );
int 	GetInput(int, int, int, char *, char *, int);

void 	break_command(void);
void 	Usage(void);

int 	ReadConfig(void);

#endif // __MAIN_H__
