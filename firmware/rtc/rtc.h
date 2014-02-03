/**************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat, Nils Eilers

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA.

***************************************************************************/

#ifndef RTC_H
#define RTC_H

#include <inttypes.h>

enum { RTC_OK, RTC_INVALID, RTC_ABSENT };

#ifdef PCTEST
    typedef char* errormsg_t;
    enum { CBM_ERROR_OK, CBM_ERROR_READ, CBM_ERROR_DRIVE_NOT_READY,
           CBM_ERROR_SYNTAX_UNKNOWN, CBM_ERROR_WRITE_ERROR };
#define set_status(dummy, msg) puts(msg)
#else
#   include "errormsg.h"
#endif

typedef struct {
    uint16_t   year;   /* 2000..2099 */
    uint8_t    month;  /* 1..12 */
    uint8_t    mday;   /* 1.. 31 */
    uint8_t    wday;   /* 1..7, 1=Monday, 2=Tuesday... */
    uint8_t    hour;   /* 0..23 */
    uint8_t    min;    /* 0..59 */
    uint8_t    sec;    /* 0..59 */
} RTC_t;


#ifdef HAS_RTC
int8_t rtc_init (void);                    // Initialize RTC
int8_t rtc_gettime (RTC_t* datim);         // Get time
int8_t rtc_settime (const RTC_t* datim);   // Set time

// parser for time commands
int8_t rtc_time(char *p, errormsg_t* errormsg);

// printf a time
void rtc_timestamp(const RTC_t* datim);
#else

static inline int8_t rtc_init(void) {
    return 0;
}

static inline int8_t rtc_gettime (RTC_t* x) {
    *x = (RTC_t) { 2012, 4, 8, 7, 6, 50, 20 };
    return 1;
}

static inline int8_t rtc_settime(const RTC_t* x) {
    return -1;
}

#define get_fattime() 0

#endif

#endif
