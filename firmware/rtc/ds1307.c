/*************************************************************************
  @file         rtc.c
  @brief        RTC DS1338 / DS1307 get and set time
  Based on rtc.c by ChaN's FatFs
 *************************************************************************/
 

#include <string.h>
#include "rtc.h"
#include "i2c.h"
#include "device.h"

uint8_t RTC_OK = 0;     /* Nonzero clock is available and contains valid time */
static uint8_t buf[8];  /* RTC R/W buffer */

int8_t rtc_gettime (RTC *rtc) 
{
  if (!i2c_read(0xD0, 0, 7, buf)) return 0;

  rtc->sec   = (buf[0] & 0x0F) + ((buf[0] >> 4) & 7) * 10;
  rtc->min   = (buf[1] & 0x0F) + (buf[1] >> 4) * 10;
  rtc->hour  = (buf[2] & 0x0F) + ((buf[2] >> 4) & 3) * 10;
  rtc->wday  = (buf[2] & 0x07);
  rtc->mday  = (buf[4] & 0x0F) + ((buf[4] >> 4) & 3) * 10;
  rtc->month = (buf[5] & 0x0F) + ((buf[5] >> 4) & 1) * 10;
  rtc->year  = 2000 + (buf[6] & 0x0F) + (buf[6] >> 4) * 10;

  return 1;
}

int8_t rtc_settime (const RTC *rtc) 
{
  buf[0] = rtc->sec / 10 * 16 + rtc->sec % 10;
  buf[1] = rtc->min / 10 * 16 + rtc->min % 10;
  buf[2] = rtc->hour / 10 * 16 + rtc->hour % 10;
  buf[3] = rtc->wday & 7;
  buf[4] = rtc->mday / 10 * 16 + rtc->mday % 10;
  buf[5] = rtc->month / 10 * 16 + rtc->month % 10;
  buf[6] = (rtc->year - 2000) / 10 * 16 + (rtc->year - 2000) % 10;
  return i2c_write(0xD0, 0, 7, buf);
}

int8_t rtc_init (void)
{
  UINT adr;

  IIC_INIT();           /* Initialize IIC function */

  /* Read RTC registers */
  if (!i2c_read(0xD0, 0, 8, buf)) return 0;     /* IIC error */

  if (buf[7] & 0x20) {  /* When data has been volatiled, set default time */
          /* Clear nv-ram. Reg[8..63] */
          memset(buf, 0, 8);
          for (adr = 8; adr < 64; adr += 8)
                  i2c_write(0x0D, adr, 8, buf);
          /* Reset time to Jan 1, '08. Reg[0..7] */
          buf[4] = 1; buf[5] = 1; buf[6] = 8;
          i2c_write(0x0D, 0, 8, buf);
  }
  return 1;
}
