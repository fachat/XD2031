//-------------------------------------------------------------------------
// Titel: XD-2031 firmware for the XS-1541 Adapter
// Funktion: Adapter to connect IEEE-488, IEC and RS232
//-------------------------------------------------------------------------
// Copyright (C) 2012  Andre Fachat <afachat@gmx.de>
// Copyright (C) 2008  Thomas Winkler <t.winkler@tirol.com>
//-------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
//-------------------------------------------------------------------------

#ifndef MAIN_H
#define MAIN_H



#define	SIZE_TERMBUF	80


#define GetDeviceNum()		(fDevice == 0 ? ieee_device : iec_device)



// EXTERNALS
extern uint8_t iec_device;						// current device# IEC
extern uint8_t ieee_device;					// current device# IEEE-488

extern uint8_t	fDevice;						// 0=IEEE, 1=IEC


//extern uint8_t		rcvBlc;						// last char
//extern uint8_t		rcvBcr;						// CR flag



#ifdef OLIMEX
  // OLIMEX Std.
  #define LED_DDR		DDRB 
  #define LED_PORT		PORTB
  #define LED_BIT		PB0
#else 
 #ifdef EMBEDIT
  // EMBEDID Std.
  #define LED_DDR		DDRD 
  #define LED_PORT		PORTD
  #define LED_BIT		PD6
 #else 
  // XS-1541 Std.
  #define LED_DDR		DDRC 
  #define LED_PORT		PORTC
  #define LED_BIT		PC0
 #endif
#endif


#define LED_SETDDR()	LED_DDR  |= _BV(LED_BIT)
#define LED_TOGGLE()	LED_PORT ^= _BV(LED_BIT)

#ifdef OLIMEX2
 #define LED_ON()		LED_PORT &= ~_BV(LED_BIT)
 #define LED_OFF()		LED_PORT |= _BV(LED_BIT)
 #define LED_TEST		(!(LED_PORT & _BV(LED_BIT)))
#else
 #define LED_ON()		LED_PORT |= _BV(LED_BIT)
 #define LED_OFF()		LED_PORT &= ~_BV(LED_BIT)
 #define LED_TEST		(LED_PORT & _BV(LED_BIT))
#endif





//ENUMS
enum tokenPar {TP_ALL,TP_BAM,TP_GCR,
			TP_D0,TP_D1,
			TP_U8,TP_U9,TP_U10,TP_U11,
			TP_SLOW,TP_BURST,TP_PARALLEL,TP_S1,TP_S2,TP_S3,
			TP_WARP,TP_TURBO,TP_BURSTLOAD,
			TP_NODUMP,TP_DUMPHEX,TP_DUMPBIN,
			TP_INTERLEAVE,
			TP_LED,TP_ECHO,
			TP_TEST
		   };
enum token {T_IEEE,T_IEC,T_DEV,T_SP,T_SD,T_RP,T_LP,T_HLP,T_CAT,T_CMD,
			T_LISTEN,T_TALK,T_UNTALK,T_UNLISTEN,T_OPEN,T_CLOSE,T_BSOUT,T_BASIN,T_ST,T_BST,
			T_DUMPFILE,T_DUMPMEM,T_DUMPBLK,T_DUMPNXTBLK,T_DUMPTRACK,T_DUMPDISK,
			T_DOWNLOAD,T_UPLOAD,T_LISTFT,T_LISTBAM,T_BACKUP,T_INQUIRE,
			T_CONFIG,T_LOADMC,T_RESET,
			T_BLANK
		   };
enum devtypes {DT_1541,DT_1571,DT_1581,DT_2030,DT_4040,DT_8050,DT_8250,
		   	   DT_DEFAULT};

enum xs1541_tok {XT_CMDS,XT_PAR1,XT_FILETYPE,XT_DEVTYPE,XT_IECPINS,XT_IEEPINS
		   	   };




#define ST_WRTO		IEEE_ST_WRTO				// write timeout
#define ST_RDTO		IEEE_ST_RDTO				// read timeout
#define ST_ATTO		IEEE_ST_ATTO				// ATN timeout
#define ST_EOI		IEEE_ST_EOI					// EOI
#define ST_DNP		IEEE_ST_DNP					// device not present




// STRUCTS
typedef struct 
{
	uint8_t	echo;				// 0=no echo
	uint8_t	led;				// LED mode 0=off, 1=on, 2=blink auto, 3=blink p
	uint8_t	led_p1;				// LED blink pulse time
	uint8_t	led_p2;				// LED blink pause time
} st_config;

extern st_config conf;




#define	Listen(x)		( (fDevice==0) ? IeeeListen(x)		: IecListen(x)	)
#define	Talk(x)			( (fDevice==0) ? IeeeTalk(x)		: IecTalk(x)	)
#define	UnListen()		( (fDevice==0) ? IeeeUnlisten()		: IecUnlisten())
#define	UnTalk()		( (fDevice==0) ? IeeeUntalk()		: IecUntalk()	)
#define	Open(x,y)		( (fDevice==0) ? IeeeOpen(x,y)		: IecOpen(x,y)	)
#define	Close(x)		( (fDevice==0) ? IeeeClose(x)		: IecClose(x)	)
#define	BsOut(x)		( (fDevice==0) ? IeeeBsout(x)		: IecBsout(x)	)
#define	BasIn(x)		( (fDevice==0) ? IeeeBasin(x)		: IecBasin(x)	)
#define	ST  			( (fDevice==0) ? ieee_status 		: iec_status 	)
#define	SetST(x)		{ if(fDevice==0) ieee_status = x;	else iec_status = x;}






void GetSpace(char **lin);
int8_t Tokenize(char **cmd, char **token, int8_t *tokenId);
char *GetTokenString(int16_t token);

int16_t ReadPar(char **cmd);
uint16_t GetHex(char **lin);
uint16_t GetDigits(char **lin, int8_t base);
int8_t GetArgNum(char **lin, uint16_t *wo);
int8_t GetArgWord(char **lin, uint16_t *wo);
int8_t GetArgByte(char **lin, uint8_t *by);
int8_t GetArgString(char **lin, char **s);
int8_t GetArgString2(char **lin, char **s);
int8_t ListHelp(void);


uint16_t BytesFree(void);

#endif
