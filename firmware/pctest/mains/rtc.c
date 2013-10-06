#include <stdio.h>
#include <time.h>
#include <string.h>
#include "rtc.h"

RTC_t time_after;

#if 0
// Compare current time with static values for 
// debug purposes will fail almost always
int8_t rtc_gettime(RTC_t* datim) {
    time_t t = time(NULL);
    struct tm *tm;

    tm = localtime(&t);
    if(!tm) return 1;

    datim->year  = tm->tm_year + 1900;
    datim->month = tm->tm_mon + 1;
    datim->mday  = tm->tm_mday;
    datim->hour  = tm->tm_hour;
    datim->min   = tm->tm_min;
    datim->sec   = tm->tm_sec;
    datim->wday  = tm->tm_wday ? tm->tm_wday : 7;

    return 0;
}
#endif

// Scriptable static version
int8_t rtc_gettime(RTC_t* datim) {
    datim->year  = 2013;
    datim->month = 10;
    datim->mday  = 5;
    datim->hour  = 14;
    datim->min   = 39;
    datim->sec   = 20;
    datim->wday  = 6;
    return 0;
}

int8_t rtc_settime(const RTC_t* datim) {
    time_after = *datim;
    return 0;
}

#define MAX_LINE 255
int main(int argc, char** argv) {
    char line[MAX_LINE + 1]; int had_a_comment = 1;

    RTC_t time_before; rtc_gettime(&time_before);
    int8_t res;
    char* output[32];

    while(fgets(line, MAX_LINE, stdin) != NULL) {
        line[strlen(line) - 1] = 0; // drop '\n'
        if(line[0] == '#') {
            if(!had_a_comment) puts("\n");
            puts(line);
            had_a_comment=1;
            continue;
        } else {
            if(!had_a_comment) puts("\n");
            had_a_comment=0;
            printf("Test: '%s'\n", line);
        }
        // --------------------------------------
        res = rtc_time(line, output);
        if(res == -1 ) {
	    printf("before: "); rtc_timestamp(&time_before);
	    printf(" after: "); rtc_timestamp(&time_after);
        } else if(res > 0) {
            printf("***** ERROR %d *****\n", res);
        } else if(res < -1) {
            printf("***** ERROR %d *****\n", res);
        }
    }

    return 0;
}
