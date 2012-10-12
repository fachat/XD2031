#ifndef RTC_DEFINED
#define RTC_DEFINED

#include "integer.h"

typedef struct {
    WORD    year;   /* 2000..2099 */
    BYTE    month;  /* 1..12 */
    BYTE    mday;   /* 1.. 31 */
    BYTE    wday;   /* 1..7 */
    BYTE    hour;   /* 0..23 */
    BYTE    min;    /* 0..59 */
    BYTE    sec;    /* 0..59 */
} RTC;

#ifdef HAS_RTC
int rtc_init (void);			/* Initialize RTC */
uint8_t RTC_OK;				/* Nonzero if RTC available and valid */
int rtc_gettime (RTC*);			/* Get time */
int rtc_settime (const RTC*);		/* Set time */
#endif

/* -------------------------------------------------------------------------- */

#ifndef HAS_RTC
  static uint8_t RTC_OK=0;			/* RTC not available */

  static int rtc_init (void) {			/* Initialize RTC */
    return 0;
  }

  static int rtc_gettime (RTC* x) {		/* Get time */	
    return 0;
  }					

  static int rtc_settime (const RTC* x) {	/* Set time */
    return 0;
  }					
#endif

#endif
