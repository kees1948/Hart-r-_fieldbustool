/*
 * 
 * This program is released under GNU Public License Version 3 or later.
 * Copyrigth 2010-2012   CWP Schoenmakers   kees2@schoen.mine.nu
 * 
 * 
 * The program is provided AS IS without any warranty. It is for 
 * field-device development purposes only and should NOT be used as an
 * official Hart(r) master application in production environments.
 * 
 */

#ifndef __HART_H__
#define __HART_H__

#include 	<stdio.h>
#include 	<fcntl.h>
#include 	<termio.h>
#include 	<stdlib.h>
#include 	<strings.h>
#include 	<string.h>
#include 	<unistd.h>
#include 	<errno.h>
#include	<ctype.h>
#include 	<signal.h>
#include 	<pthread.h>
#include 	<curses.h>
#include 	<sys/poll.h>
#include	<sys/time.h>
#include	<linux/serial.h>
#include	<errno.h>
#include	<mysql/mysql.h>
#include	<mysql/mysqld_error.h>


typedef unsigned char BYTE;

typedef struct
{
    unsigned short dt;
    unsigned short cn;
    unsigned char rq;
    unsigned char rs;
    unsigned char sz;
    char pt[6];
    unsigned char rx;
    unsigned char ry;
    char tt[20];
    unsigned short tn;
} HT;

#define HART_GAP_TIME		17                                                       // 2 chars
#define	RT2_TIME			74
#define	RT1_TIMEP			303
#define	RT1_TIMES			376
#define	STO_TIME			257

#define	HART_FRAME_RECVD	0x80
#define	HART_LADDR_LEN		5
#define	HART_EXP_LEN		3
#define	HART_DATABUF_LEN	253

#define	HART_PREAMBLE		0xff
#define	FLUSHB				0xff
#define	HART_D_STX			0x02
#define	HART_D_ACK			0x06
#define	HART_D_BACK			0x01
#define	HART_D_LONG_FRAME	0x80
#define	HART_DEL_MASK		0x07
#define	HART_EXP_MASK		0x60
#define	HART_PHYS_MASK		0x18
#define	HART_A0_PRIMARY		0x80
#define	HART_A0_SCEUNDARY	0x00
#define	HART_A0_BURSTFLAG	0x40

#define	CODOFL				128+2
#define	COCKSM				128+8
#define	COFRER				128+16
#define	COOVRN				128+32
#define	COPARE				128+64
#define	CODABT				128

#define	DB_MATCH			0
#define	DB_REQ_DATA			1
#define	DB_RSP_DATA			2
#define	DB_BURSTDEV			128

#define	PE					1
#define	OE					2
#define	FE					4

#define NOBODY 				0
#define PRIMARY				1
#define	SECUNDARY			2

#define		MAXPOLL		15

#define NUM_PREAM			18

#define	READ_DATA_OFFSET	2                                                        // read data offset in receivebuffer
#define IND_CMD_OFFSET		2														// additional 2 bytes to skip

#define	ZERO				0

typedef struct
{
    unsigned char longaddress[HART_LADDR_LEN];
} SI;

#endif
