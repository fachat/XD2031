/*************************************************************************
  @file         rtc.c
  @brief        RTC DS1338 / DS1307 get and set time
  Based on rtc.c by ChaN's FatFs
 *************************************************************************/

#include "rtc.h"
#include "i2c.h"
#include "debug.h"

// Get time from real time clock
// RTC_OK: valid time
// RTC_INVALID: RTC present, but time unset, delivers default date
// RTC_ABSENT: No RTC found
int8_t rtc_gettime (RTC_t *rtc) {
  uint8_t buf[10];

  if (i2c_read(0xD0, 0, 10, buf)) return 2;

  if ((buf[7] & 0x20) || (buf[8] != 'X') || (buf[9] != 'D')) {
    // DS1338 addr 7 bit 5 is high on invalid time
    // DS1307 will always read out as 0
    *rtc = (RTC_t) { 2012, 4, 8, 7, 6, 50, 20 };
    return 1;
  }

  rtc->sec   = (buf[0] & 0x0F) + ((buf[0] >> 4) & 7) * 10;
  rtc->min   = (buf[1] & 0x0F) + (buf[1] >> 4) * 10;
  rtc->hour  = (buf[2] & 0x0F) + ((buf[2] >> 4) & 3) * 10;
  rtc->wday  = (buf[3] & 0x07);
  rtc->mday  = (buf[4] & 0x0F) + ((buf[4] >> 4) & 3) * 10;
  rtc->month = (buf[5] & 0x0F) + ((buf[5] >> 4) & 1) * 10;
  rtc->year  = 2000 + (buf[6] & 0x0F) + (buf[6] >> 4) * 10;

  return 0;
}

// Set time
// returns 0 on success, non-null on errors
int8_t rtc_settime (const RTC_t *rtc) {
  uint8_t buf[10];

  buf[0] = rtc->sec / 10 * 16 + rtc->sec % 10;
  buf[1] = rtc->min / 10 * 16 + rtc->min % 10;
  buf[2] = rtc->hour / 10 * 16 + rtc->hour % 10;
  buf[3] = rtc->wday & 7;
  buf[4] = rtc->mday / 10 * 16 + rtc->mday % 10;
  buf[5] = rtc->month / 10 * 16 + rtc->month % 10;
  buf[6] = (rtc->year - 2000) / 10 * 16 + (rtc->year - 2000) % 10;
  buf[7] = 0;
  buf[8] = 'X';
  buf[9] = 'D';
  return i2c_write(0xD0, 0, 10, buf);
}

int8_t rtc_init (void) {
  RTC_t rtc;
  uint8_t res;

  res = rtc_gettime(&rtc);
  debug_puts("RTC: ");
  switch(res) {
    case 0:
      rtc_timestamp(&rtc);
      break;
    case 1:
      debug_puts("invalid\n");
      break;
    case 2:
      debug_puts("not found\n");
      break;
    default:
      debug_printf("%d?!\n", res);
  }
  return res;
}
