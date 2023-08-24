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

extern int verbose;

/*-GET_EVENT_TIME-------------------------------------------------*/
int get_event_time(int *sec, int *nsec, int BASE1)
{
	struct timespec sleep, tp;
	struct tm localtime;
	time_t calandertime;
	int		 temp,temp174,temp178,temp17c;
	int		 year,month,day,hour,minute,second,nsecond;

  // READ THE TIME
  //Check event status flag
  if (BASE1 != NULL) {
    if ((*((uint32_t*)(BASE1+0xfe))& 0x01) != 0x01) return -1;

    if (verbose > 1) printf("Event status flag enabled\n");

    //read time of last event 
    temp174 = *((uint32_t*)(BASE1+0x174));
    temp178 = *((uint32_t*)(BASE1+0x178));
    temp17c = *((uint32_t*)(BASE1+0x17c));

    //clear event status flag so new event can be captured
    *((uint32_t*)(BASE1+0xf8)) |= 0x01;
    if (verbose > 1) printf("Clear event status flag\n");
    temp = temp17c;
    year = 1000*((temp & 0xf000) >> 12) + 100*((temp & 0xf00) >> 8) +
            10*((temp & 0xf0) >> 4) + 1*((temp & 0xf));
		localtime.tm_year = year-1900;

		//day
		temp = temp178;
		day = 100*((temp & 0xf000000) >> 24) + 10*((temp & 0xf00000) >> 20) +
          1*((temp & 0xf0000)>>16);

    dayno2mmdd(year, day, &month, &day);

		localtime.tm_mday   = day;
		localtime.tm_mon    = month;
		localtime.tm_isdst  = 0;
		localtime.tm_gmtoff = 0;

		//hour
		temp = temp178;
		hour = 10*((temp & 0xf000) >> 12) + 1*((temp & 0xf00) >> 8);
		localtime.tm_hour = hour;

		//minute
		minute = 10*((temp & 0xf0) >> 4) + 1*((temp & 0xf));
		localtime.tm_min = minute;
		
		//second
		temp = temp174;
		second = 10*((temp & 0xf0000000) >> 28) + 1*((temp & 0xf000000) >> 24);
		localtime.tm_sec = second;

		//nano second
		nsecond = 100*((temp17c & 0xf00000) >> 20) + 1000000*((temp & 0xf000) >> 12)
              + 100000*((temp & 0xf00) >> 8) + 10000*((temp & 0xf0) >> 4)
              + 1000*((temp & 0xf));
		nsecond += ( 100000000*((temp & 0xf00000) >> 20) +
                  10000000*((temp & 0xf0000) >> 16) );

		calandertime = mktime(&localtime);
		*sec  = calandertime;
		*nsec = nsecond;
  } else {
    clock_gettime(CLOCK_REALTIME,&tp);
    *nsec = tp.tv_nsec;
    *sec  = tp.tv_sec;
    calandertime = tp.tv_sec;
  }

  return calandertime;
}

