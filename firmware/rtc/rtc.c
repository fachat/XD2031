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

// This file holds routines common for all types of
// real time clocks, e.g. the time parser

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef PCTEST
#include <time.h>
#else
#include "errormsg.h"
#endif

#include "rtc.h"


static uint8_t day_of_week(uint16_t y, uint8_t m, uint8_t d) {
// Algorithm by Tomohiko Sakamoto
    const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    uint8_t dow;

    y -= m < 3;
    dow =  (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;

    return (dow ? dow : 7);
}

static const char *rtc_name_of_day[7] =
    { "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };


/*

Two syntax schemes are supported:
- TI for BASIC 4.0 ti$ compatibility
- T according a ISO 8601 subset

T[R|W]-[A|B|D] CMD syntax is not yet supported.

If the command is called without parameters, the current
date and time is written to the status channel.
If the command is called as TI, only the time is written to
the status channel in a form that is suitable to assign to
BASIC 4's ti$.

Calling with parameters is used to set date and/or time.
A ISO 8601 subset is used, which may be preceded by a three
letter abbreviation of the day of the week, e.g. FRI.

If a valid time and/or date string is found, the given
values of datim are updated.
On errors, datim remains unchanged.

If the calendar date is set but the name of the day is not,
the day of the week is computed and set also.

If the command is called as 'T' (not TI), the separator 'T' is
required to indicate a following time constant.

It is possible to omit separators. Suggested separators
are '-' for calendar dates and ':' for times, but any
character with a code less than 64 will work as well, with the
exception of NUL and digits.

*/

static int8_t isdelim(char c) {
    if(c == 0) return 0;
    if(isdigit(c)) return 0;
    if(c < 64) return 1;
    return 0;
}

static char* skip_spaces(char *p) {
    while(*p) if(*p == ' ') p++; else break;
    return p;
}

int8_t rtc_time(char *p, errormsg_t* errormsg) {
    // returns
    // -1 OK, time was set
    //  0 OK, time was read
    // >0 CBM_ERROR_*

    //printf("rtc_time: '%s'\n", p);

    char c;
    uint8_t day_of_week_given = 0;
    uint8_t date_given = 0;
    uint8_t time_given = 0;
    uint8_t digits_before_delim = 0;
    char time_chars[7] = "000000";
    char date_chars[9];
    uint8_t date_digits = 0;
    uint8_t time_digits = 0;
    uint8_t called_as_ti = 0;

    RTC_t datim;
    char buf[32]; // buffer for formatted date/time output

    p++; // Skip 'T'

    if(*p == 'I') {
        called_as_ti = 1;
        p++;
    } else {

        // Get day of week if given
        p = skip_spaces(p);
        for(uint8_t d = 7; d; d--) {
            if(!strncmp(p, rtc_name_of_day[d - 1], 3)) {
                day_of_week_given = d;
                p += 3;
                break;
            }
        }

        // Get calendar date if given
        // TODO: check length before delimiter
        p = skip_spaces(p);
        date_chars[0] = 0;
        char *date_ptr = date_chars;
        while(1) {
            c = *p;
            if(!c) break;
            if(isdigit(c)) {
                p++;
                date_given = 1;
                if(date_digits < 8) {
                    date_digits++;
                    *date_ptr++ = c;
                } else {
                    printf("String too long\n");
                    return CBM_ERROR_SYNTAX_UNKNOWN; // date string too long;
                }
            } else {
                if(isdelim(c)) {
                    p++;
                } else {
                    break;
                }
            }
        }
        *date_ptr = 0;

    } // called_as_ti


    // Get time if given
    p = skip_spaces(p);
    if(*p) {
        time_given = 1;

        // if called as T (not TI), a 'T' is required
        // to indicate a time constant is following
        if(!called_as_ti) {
            if(*p == 'T') p++;
            else {
                printf("expected T\n");
                return CBM_ERROR_SYNTAX_UNKNOWN;
            }
        }

        time_digits = 0;
        char *time_ptr = time_chars;
        while(1) {
            p = skip_spaces(p);
            c = *p++;
            if(!c) break;
            if(isdigit(c)) {
                digits_before_delim++;
                if(time_digits++ < 6) {
                    *time_ptr++ = c;
                } else {
                    printf("Time string too long\n");
                    return CBM_ERROR_SYNTAX_UNKNOWN;
                }
            } else {
                if (isdelim(c)) {
                    if(digits_before_delim) {
                        if(digits_before_delim > 2) {
                            printf("Value too long\n");
                            return CBM_ERROR_SYNTAX_UNKNOWN;
                        }
                        digits_before_delim = 0;
                    } else {
                        printf("Unexpected delimiter\n");
                        return CBM_ERROR_SYNTAX_UNKNOWN;
                    }
                } else {
                    printf("Unexpected character\n");
                    return CBM_ERROR_SYNTAX_UNKNOWN;
                }
            }
        }
    }

    int8_t res = rtc_gettime(&datim);
    if(res == RTC_ABSENT) return CBM_ERROR_DRIVE_NOT_READY; // RTC not found

    // If no parameters were given, output date/time
    if(!(day_of_week_given || date_given || time_given)) {
        if(res == RTC_INVALID) return CBM_ERROR_READ;       // RTC found but invalid

        if(called_as_ti) {
            sprintf(buf, "%02u%02u%02u",
                datim.hour, datim.min, datim.sec);
        } else {
            sprintf(buf, "%s %04u-%02u-%02uT%02u:%02u:%02u",
                rtc_name_of_day[datim.wday-1],
                datim.year, datim.month, datim.mday, datim.hour,
                datim.min, datim.sec);
        }
        set_status(errormsg, buf);

        return 0;
    }

    // Attempting to set date and/or time, do some basic checks

    if(date_given) {
        if(date_digits != 8) {
            printf("Expected date as YYYY-MM-DD\n");
            return CBM_ERROR_SYNTAX_UNKNOWN;
        }
        datim.mday = atoi(date_chars + 6);
        date_chars[6] = 0;
        datim.month = atoi(date_chars + 4);
        date_chars[4] = 0;
        datim.year = atoi(date_chars);
        if((datim.mday  <    1) || (datim.mday  >   31) ||
           (datim.month <    1) || (datim.month >   12) ||
           (datim.year  < 2000) || (datim.year  > 2099)) {
            printf("Illegal date\n");
            return CBM_ERROR_SYNTAX_UNKNOWN;
        }
        if(!day_of_week_given) {
            day_of_week_given = day_of_week(datim.year, datim.month, datim.mday);
        }
    }

    if(time_given) {
        if((time_digits != 4) && (time_digits != 6)) {
            printf("Expected HH:MM[:SS]\n");
            return CBM_ERROR_SYNTAX_UNKNOWN;
        }
        datim.sec = atoi(time_chars + 4);
        time_chars[4] = 0;
        datim.min = atoi(time_chars + 2);
        time_chars[2] = 0;
        datim.hour = atoi(time_chars);
        if((datim.sec > 59) || (datim.min > 59) || (datim.hour > 23)) {
            printf("Illegal time\n");
            return CBM_ERROR_SYNTAX_UNKNOWN;
        }
    }

    if(day_of_week_given) datim.wday = day_of_week_given;

    // Apply given parameters
    if(rtc_settime(&datim)) return CBM_ERROR_WRITE_ERROR;
    return -1;
}


void rtc_timestamp(const RTC_t* datim) {
     printf("%s %04u-%02u-%02uT%02u:%02u:%02u\n",
         rtc_name_of_day[datim->wday-1],
         datim->year, datim->month, datim->mday,
         datim->hour, datim->min, datim->sec);
}
