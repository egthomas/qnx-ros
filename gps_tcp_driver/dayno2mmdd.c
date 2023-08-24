/*
 * Convert year and day number to month and day. Note:
 *   month: [0-11]
 *   day  : [1-31]
 *
 * 20190124 SGS seeking a more efficient and easier to understand conversion
 *              than what is currently in several GPS driver functions:
 *                   _get_compare_time.c
 *                   _get_event_time.c
 *                   _get_software_time.c
 *
 */
#include <stdio.h>
#include "dayno.h"

int dayno2mmdd(int year, int dayno, int *month, int *day)
{
  int leap;
  int k;
  int days[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};

  leap = leap_correct(year);

  if ((dayno <= 0) || (dayno > days[12]+leap)) {
    printf("Day out of range %d\n", dayno);
    return (-1);
  }

  for (k=0;k<12;k++) {
    /* add a day if leap year and month > Jan */
    if (dayno <= days[k+1]+(leap*(k>0))) {
      *month = k;
      *day = dayno-(days[k]+leap*(k>1));
      break;
    }
  }

  return (0);
}

int leap_correct(int year)
{
  if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) return (1);
  else return (0);
}

int leap_not(int year)
{
  if (year % 4 == 0) return (1);
  else return (0);
}

