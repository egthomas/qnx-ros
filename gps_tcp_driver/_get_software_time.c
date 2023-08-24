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
#include "rtypes.h"
#include "_prog_conventions.h"

/*-GET_SOFTWARE_TIME-------------------------------------------------*/
int get_software_time(int *sec, int *nsec, int BASE1)
{
  struct timespec sleep, tp;
  struct tm localtime;
  time_t calandertime;
  int    temp;
  int    year,month,day,hour,minute,second,nsecond;

  // READ THE TIME
  // poll status reg an wait 10 us
  if (BASE1 != NULL) {
    *((uint08*)(BASE1+0xfc)) = 0x0; // write any value to update time and pos
    sleep.tv_sec  = 0;
    sleep.tv_nsec = 10000;

    // should check that it is okay to read...
    //temp = *((uint32_t*)(BASE1+0xfc));
    //temp &= 0x00700000;
    //temp = temp >> 20;
    //if (temp == 7) printf(" GPS READY\n");

    temp = *((uint32_t*)(BASE1+0x104));

    year = 1000*((temp & 0xf0000000) >> 28) + 100*((temp & 0xf000000) >> 24) +
           10*((temp & 0xf00000) >> 20) + 1*((temp & 0xf0000) >> 16);
    localtime.tm_year = year-1900;

    day = 100*((temp & 0xf00) >> 8) + 10*((temp & 0xf0) >> 4) + (temp & 0x0f);

    dayno2mmdd(year, day, &month, &day);
    
    localtime.tm_mday   = day;
    localtime.tm_mon    = month;
    localtime.tm_isdst  = 0;
    localtime.tm_gmtoff = 0;

    //hour
    temp = *((uint32_t*)(BASE1+0x100));
    hour = 10*((temp & 0xf0000000) >> 28) + 1*((temp & 0xf000000) >> 24);
    localtime.tm_hour = hour;

    //minute
    minute = 10*((temp & 0xf00000) >> 20) + 1*((temp & 0xf0000) >> 16);
    localtime.tm_min = minute;
    
    //second
    second = 10*((temp & 0xf000) >> 12) + 1*((temp & 0xf00) >> 8);
    localtime.tm_sec = second;

    //nano second
    temp = *((uint32_t*)(BASE1+0xfc));
    nsecond = 100*((temp & 0xf0000000) >> 28) + 1000000*((temp & 0xf000) >> 12)
              + 100000*((temp & 0xf00) >> 8) + 10000*((temp & 0xf0) >> 4)
              + 1000*((temp & 0xf));
    temp = *((uint32_t*)(BASE1+0x100));
    nsecond += ( 100000000*((temp & 0xf0) >> 4) + 10000000*(temp & 0xf) );

    calandertime = mktime(&localtime);
    *sec  = calandertime;
    *nsec = nsecond;
  } else {  /* BASE1 is NULL */
    clock_gettime(CLOCK_REALTIME, &tp);
    *nsec = tp.tv_nsec;
    *sec  = tp.tv_sec;
    calandertime = tp.tv_sec;
}                                     

  return calandertime;
}

