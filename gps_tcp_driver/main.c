#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#ifdef __QNX__
  #include <hw/pci.h>
  #include <hw/inout.h>
  #include <sys/neutrino.h>
  #include <sys/iofunc.h>
  #include <sys/dispatch.h>
#endif
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "control_program.h"
#include "global_server_variables.h"
#include "include/registers.h"
#include "include/_refresh_state.h"
#include "utils.h"

int	sock, msgsock;
int sockaccepted=0;
int tc_count=0;
int configured=0, locked=0;
unsigned char *BASE0=NULL, *BASE1=NULL;
int IRQ;
int intid;
struct sigevent interruptevent;
struct GPSStatus displaystat;
struct timespec timecompare, event;
int RateSynthInterrupt=0, EventInterrupt=0, TimeCompareInterrupt=0;
int timecompareupdate=0, eventupdate=0;
pthread_t int_thread_id, refresh_thread_id;
int verbose=2;
int wait_for_lock=0;
pthread_mutex_t gps_state_lock;

int set_time_compare(int mask, struct timespec *nexttime);
int set_time_compare_register(int mask, struct timespec *nexttime);

void graceful_cleanup(int signum)
{
  close(msgsock);
  close(sock);
  exit(0);
}
/*-MAIN--------------------------------------------------------------*/
 main() {
  int temp, pci_handle, i, frame_size, status, temp2, ch;
  unsigned int *mmap_io_ptr, CLOCK_RES;
  struct timespec now, start_p, stop_p;
  struct timespec new, old;
  int gpssecond, gpsnsecond;
  struct timeval tv;
  struct DriverMsg msg;
	// socket and message passing variables
  char datacode;
  int	rval, buf; 
  int first_gps_lock;
  fd_set rfds, efds;

  signal(SIGINT, graceful_cleanup);

  pthread_mutex_init(&gps_state_lock, NULL);

  /* SET THE SYSTEM CLOCK RESOLUTION AND GET THE START TIME OF THIS PROCESS */
  new.tv_nsec=10000;
//	sleep.tv_sec=0;
//	sleep.tv_nsec=10000;
#ifdef __QNX__
/* commented out in FH code
  temp = ClockPeriod(CLOCK_REALTIME,&new,0,0);
  if (temp==-1) {
    perror("Unable to change system clock resolution");
  }
  temp = clock_gettime(CLOCK_REALTIME, &start_p);
  if (temp==-1) {
    perror("Unable to read sytem time");
  }
  temp = ClockPeriod(CLOCK_REALTIME,0,&old,0);
  CLOCK_RES = old.tv_nsec;
  if (verbose > 0) {
    fprintf(stderr,"CLOCK_RES: %d\n", CLOCK_RES);
    fflush(stderr); 
  }
*/
#endif

  /* OPEN THE PLX9656 AND GET LOCAL BASE ADDRESSES */
//	clock_gettime(CLOCK_REALTIME, &start);
  temp = _open_PLX9050(&BASE0, &BASE1, &pci_handle, &mmap_io_ptr, &IRQ);
  if (temp==-1) {
    fprintf(stderr, "	PLX9695 configuration failed\n");
    configured = 0;
  } else {
    if (verbose > 0) fprintf(stderr,"IRQ is %d\n",IRQ);
    configured = 1;
  }
  fprintf(stderr,"BASE0 %p  BASE1 %p\n",BASE0,BASE1);
  fflush(stderr);
  displaystat.mdrift = 0;

  //INITIALIZE CARD
  if (configured) {
    // set card for synchronized generator mode, GPS reference enabled
    // Daylight Savings Time Disabled	
    *((uint32_t*)(BASE1+0x118)) = 0x90000021; //x90 to address x11B produced
                                            // 10 MHz reference signal output
    //clear time compare register
    //now.tv_sec += 0;  // SGS: this HAS to be a mistake
    now.tv_sec  = 0;
    now.tv_nsec = 0;
    temp = set_time_compare(0, &now);
    // clear time compare event
    if (configured) *((uint08*)(BASE1+0xf8)) |= 0x02;

    //SET INITIAL TIME COMPARE TIME
  //*((uint32*)(BASE1+0x128)) = 2;    // value for FH
  //*((uint32*)(BASE1+0x128)) = 10;   // set rate synthesiser rate to a default
    *((uint32_t*)(BASE1+0x128)) = 2;  // set rate synthesiser rate to a default
                                      //   of 100ms (10pps)
  //*((uint32*)(BASE1+0x12c)) = 0x0200ff00;  // value for CV
    *((uint32_t*)(BASE1+0x12c)) = 0x02000f00;// x12e: event trigger on external
                                             //       event, falling edge 
    // x12d: rising edge rate synthesizer on pin 6, 
    // x12f: rate generator on code-out 

    fprintf(stdout,"****************************************");
    fprintf(stdout,"****************************************\n");
    fprintf(stdout,"GPS Card Registers\n");
    fprintf(stdout,"Rate synth:   0x%08x\n",*((uint32_t*)(BASE1+0x128)));
    fprintf(stdout,"Cntl:         0x%08x\n",*((uint32_t*)(BASE1+0x12c)));
    fprintf(stdout,"Misc ctrl:          0x%02x\n",*((uint8_t*)(BASE1+0x12C)));
    fprintf(stdout,"Rate Sync ctrl:     0x%02x\n",*((uint8_t*)(BASE1+0x12D)));
    fprintf(stdout,"Event Capture ctrl: 0x%02x\n",*((uint8_t*)(BASE1+0x12E)));
    fprintf(stdout,"Code Out cntrl:     0x%02x\n",*((uint8_t*)(BASE1+0x12F)));
    fprintf(stdout,"****************************************");
    fprintf(stdout,"****************************************\n");
    fprintf(stdout,"Antenna Status:     0x%02x\n",*((uint8_t*)(BASE1+0xFE)));
    fprintf(stdout,"Backgrnd Test Stat: 0x%02x\n",*((uint8_t*)(BASE1+0x11C)));
    fprintf(stdout,"Satellite A:        0x%02x\n",*((uint8_t*)(BASE1+0x198)));
    fprintf(stdout,"Satellite B:        0x%02x\n",*((uint8_t*)(BASE1+0x19C)));
    fprintf(stdout,"Satellite C:        0x%02x\n",*((uint8_t*)(BASE1+0x1A0)));
    fprintf(stdout,"Satellite D:        0x%02x\n",*((uint8_t*)(BASE1+0x1A4)));
    fprintf(stdout,"Satellite E:        0x%02x\n",*((uint8_t*)(BASE1+0x1A8)));
    fprintf(stdout,"Satellite F:        0x%02x\n",*((uint8_t*)(BASE1+0x1AC)));
    fprintf(stdout,"****************************************");
    fprintf(stdout,"****************************************\n");
 
    //set flag to indicate default trigger mode of rate synthesizer triggers
    //displaystat.triggermode=8;

    //start the interrupt handler
  }

  // enable interrupts
  RateSynthInterrupt = 0;
  EventInterrupt = 0;
//        if(configured && IRQ < 16) {
//          if (verbose > 0) printf("starting int thread\n");
//	  pthread_create(&int_thread_id, NULL, int_thread, NULL);
//        } 
//        if(configured) {
//          if (verbose > 0) printf("enabling event interrupt\n");
//	  *((uint08*)(BASE1+0xf8))=0x00;
//	  *((uint08*)(BASE1+0xf8))=0xcf;
//        }
  //start thread to refresh display
  if (verbose > 0) {
    fprintf(stderr,"starting refresh thread\n");
    fflush(stderr);
  }
  pthread_create(&refresh_thread_id, NULL, refresh_state, NULL);
  //set terminal for input
//	halfdelay(1);

	//for (i=0;i<1000;i++)
  temp = clock_gettime(CLOCK_REALTIME, &start_p);
  //open tcp socket	
  sock = tcpsocket(GPS_HOST_PORT);
  temp = 1;
	//ioctl(sock,FIONBIO,&temp);
	//cntl(sock,F_SETFL,O_NONBLOCK);
  listen(sock,5);
  first_gps_lock = 0;
  while (1) {
    if (!first_gps_lock && configured) {
      pthread_mutex_lock(&gps_state_lock);
      get_state();
      first_gps_lock |= displaystat.gps_lock;
      pthread_mutex_unlock(&gps_state_lock);
      if (!first_gps_lock) {
        fprintf(stderr,"No GPS Lock Yet\n");
        fflush(stderr);
        locked = 0;
        if (wait_for_lock) {
          fprintf(stderr,"Waiting a few seconds for GPS Lock %d %d %d %d\n",
                  displaystat.antenna, displaystat.reference_lock,
                  displaystat.phase_lock, displaystat.gps_lock);
          fflush(stderr);
          sleep(10);
          continue; 
        } else {
          fprintf(stderr,"GPS Not Waiting for Lock, "
                        "willing to talk to ROS now\n");
          fflush(stderr);
          first_gps_lock = 1; 
          locked = 1;
        }
      } else {
        fprintf(stderr,"GPS Lock, willing to talk to ROS now\n");
        fflush(stderr);
        locked = 1;
      } 

    }
    rval = 1;
    msgsock = accept(sock, 0, 0);
    if (verbose > 0) printf("accepting socket!!!!!\n");
    if (msgsock==-1) {
      perror("accept FAILED!");
      return EXIT_FAILURE;
    } else
      while (rval >= 0) {
        FD_ZERO(&rfds);
        FD_SET(msgsock, &rfds); //Add msgsock to the read watch
        FD_ZERO(&efds);
        FD_SET(msgsock, &efds);  //Add msgsock to the exception watch
        /* Wait up to five seconds. */
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        if (verbose > 0) printf("%d Entering Select\n",msgsock);
        rval = select(msgsock+1, &rfds, NULL, &efds, &tv);
        if (verbose > 0) printf("%d Leaving Select %d\n",msgsock,rval);
        /* Don???t rely on the value of tv now! */
        if (FD_ISSET(msgsock,&efds)) {
          if (verbose > 0)
            printf("Exception on msgsock %d ...closing\n",msgsock);
          break;
        }
        if (rval == -1) perror("select()");
        rval = recv(msgsock, &buf, sizeof(int), MSG_PEEK); 
        if (verbose > 0) printf("%d PEEK Recv Msg %d\n",msgsock,rval);
        if (rval==0) {
          if (verbose > 0)
            printf("Remote Msgsock %d client disconnected ...closing\n",
                    msgsock);
          break;
        } 
        if (rval<0) {
          if (verbose > 0) printf("Msgsock %d Error ...closing\n",msgsock);
          break;
        } 
        if ( FD_ISSET(msgsock,&rfds) && rval>0 ) {
          if (verbose > 0)
            printf("Data is ready to be read\n%d Recv Msg\n",msgsock);
          rval = recv_data(msgsock,&msg,sizeof(struct DriverMsg));
          datacode=msg.type;
          if (verbose > 0) printf("\nmsg code is %c\n", datacode);
          locked |= displaystat.gps_lock;

          switch (datacode) {

            case GPS_GET_SOFT_TIME:
              if (verbose > 1)
                fprintf(stderr,"GPS_GET_SOFT_TIME: Configured: %d Locked: %d\n",
                        configured,locked); 

              gpssecond  = 0;
              gpsnsecond = 0;
              if (configured && locked) {
                fprintf(stderr, "  SOFT\n");
                msg.status = get_software_time(&gpssecond,&gpsnsecond,BASE1);
              } else {
                fprintf(stderr, "  NOT YET\n");
                msg.status = -1;
              }
              if (verbose > 1) {
                fprintf(stderr," %s\n", ctime(&gpssecond)); 
                fflush(stderr);
              }
					    rval = send_data(msgsock,&gpssecond, sizeof(int));
					    rval = send_data(msgsock,&gpsnsecond, sizeof(int));
              rval = send_data(msgsock, &msg, sizeof(struct DriverMsg));
              break;

            case GPS_GET_EVENT_TIME:
              gpssecond  = 0;
              gpsnsecond = 0;
              if (verbose > 1) fprintf(stderr,"GPS_GET_EVENT_TIME: ");

              if (configured && locked) {
                if (verbose > 1) fprintf(stderr,"GPS  ");
                msg.status = get_event_time(&gpssecond,&gpsnsecond,BASE1);
              } else {
                if (verbose > 1) fprintf(stderr,"SOFT ");
                msg.status = get_software_time(&gpssecond,&gpsnsecond,BASE1);
              }
              if (verbose > 1) {
                fprintf(stderr,"EVENT: Configured: %d Locked: %d\n",
                          configured,locked); 
                fprintf(stderr," %s\n", ctime(&gpssecond)); 
                fflush(stderr);
              }
              rval = send_data(msgsock,&gpssecond, sizeof(int));
              rval = send_data(msgsock,&gpsnsecond, sizeof(int));
              rval = send_data(msgsock, &msg, sizeof(struct DriverMsg));
              break;

            case GPS_SET_TRIGGER_RATE:
              if (verbose > 1) fprintf(stderr,"GPS_SET_TRIGGER_RATE: ");
              rval = recv_data(msgsock,&displaystat.ratesynthrate,sizeof(int));
              if (verbose > 1)
                fprintf(stderr, " Set Rate: %d\n", displaystat.ratesynthrate);
/*              if (configured && locked) */
              if (configured) {

                // set rate synthesizer rate
                *((uint32_t*)(BASE1+0x128)) = displaystat.ratesynthrate;

                // load rate synthesizer rate 
//					  *((uint32*)(BASE1+0x12c))|=0x00000200;  // CV value
    					  *((uint32_t*)(BASE1+0x12c)) |= 0x00000f00;
              }
              rval = send_data(msgsock, &msg, sizeof(struct DriverMsg));
              break;

            case GPS_GET_HDW_STATUS:
              //this function does not yet work
              if (verbose > 1) fprintf(stderr, "GPS_GET_HDW_STATUS: ");
              pthread_mutex_lock(&gps_state_lock);
              get_state();
              pthread_mutex_unlock(&gps_state_lock);
              rval = send_data(msgsock, &displaystat, sizeof(struct GPSStatus));
              rval = send_data(msgsock, &msg, sizeof(struct DriverMsg));
              break;

            case GPS_TRIGGER_NOW:
              //this function does not yet work
              if (verbose > 1) fprintf(stderr, "GPS_TRIGGER_NOW: ");
              status = 0;
              rval = send_data(msgsock,&status, sizeof(int));
              break;

            default:	
            if (verbose > 0)
              fprintf(stderr,"BAD CODE: %c : %d\n",datacode,datacode);
            break;
          }
        }
      }
  }
  /* END TEST CODE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

  /* CLOSE THE PLX9656 AND CLEAR ALL MEMORY MAPS */
  temp = _close_PLX9656(pci_handle, BASE0, BASE1, mmap_io_ptr);
  printf("close:		0x%x\n", 3);

  /* READ SYSTEM TIME AND END OF PROCESS, AND PRINT TOTAL TIME TO RUN */
  temp = clock_gettime(CLOCK_REALTIME, &stop_p);
  if (temp==-1) {
    perror("Unable to read sytem time");
  }
//	time=(float)(stop_p.tv_nsec-start_p.tv_nsec);
//	fprintf(stderr, "TOTAL TIME: %f\n",time);

}

