#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <ncurses.h>
#include <unistd.h>
#include "rtypes.h"
#include "_prog_conventions.h"

/*-GET_COMPARE_TIME-------------------------------------------------*/
int get_compare_time(int *sec, int *nsec, int BASE1)
{
  struct timespec systime;
  struct tm localtime;
  struct tm *syslocaltime;
  time_t calandertime;
  int    temp,temp138,temp13c;
  int    year,month,day,hour,minute,second,nsecond,yday;
  FILE   *fp;

  // READ THE TIME
  //get year from system time
  temp = clock_gettime(CLOCK_REALTIME,&systime);
  if (BASE1 != NULL) {
    //read time of last event 
    temp138 = *((uint32_t*)(BASE1+0x138));
    temp13c = *((uint32_t*)(BASE1+0x13c));
    syslocaltime = gmtime(&systime.tv_sec);
    localtime.tm_year = syslocaltime->tm_year;
    year = localtime.tm_year;

    //day
    temp = temp13c;
    day = 100*((temp & 0xf000000) >> 24) + 10*((temp & 0xf00000) >> 20) +
          1*((temp & 0xf0000)>>16) ;
    yday = day;

    dayno2mmdd(year, day, &month, &day);

    localtime.tm_mday   = day;
    localtime.tm_mon    = month;
    localtime.tm_isdst  = 0;
    localtime.tm_gmtoff = 0;

    //hour
    temp = temp13c;
    hour = 10*((temp & 0xf000) >> 12) + 1*((temp & 0xf00) >> 8);
    localtime.tm_hour = hour;

    //minute
    minute = 10*((temp & 0xf0) >> 4) + 1*((temp & 0xf));
    localtime.tm_min = minute;

    //second
    temp = temp138;
    second = 10*((temp & 0xf0000000) >> 28) + 1*((temp & 0xf000000) >> 24);
    localtime.tm_sec = second;

    //nano second
    nsecond = 1000000*((temp & 0xf000) >> 12) + 100000*((temp & 0xf00) >> 8) +
              10000*((temp & 0xf0) >> 4) + 1000*((temp & 0xf));
    nsecond += ( 100000000*((temp & 0xf00000) >> 20) +
                  10000000*((temp & 0xf0000) >> 16) );
  
    calandertime = mktime(&localtime);
    *sec  = calandertime;
    *nsec = nsecond;
  } else {  /* BASE1 is NULL */
    *sec  = systime.tv_sec;
    *nsec = systime.tv_nsec; 
    calandertime = systime.tv_sec; 
  }

return calandertime;
}

