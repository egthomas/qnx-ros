#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#ifdef __QNX__
  #include <hw/pci.h>
  #include <hw/inout.h>
  #include <sys/neutrino.h>
  #include <sys/mman.h>
#endif
#include "registers.h"

#define SWITCHES 0
#define ATTEN    1
#define READ     0
#define WRITE    1
#define ON       1
#define OFF      0

#define NEW_PMAT 1 

#define CARDS 20 
#define PHASECODES 8192 
#define FREQS 1500

int sock=-1;
int verbose=2;
char *hostip="137.229.27.122";
char *file_prefix="phasing_cal";
char *file_ext=".dat";
char filename[120];
char *dir="/tmp/";
FILE *calfile=NULL;
int port=23;
char command[80];
char radar_name[80];
char freq_start[10]="8E6";
char freq_stop[10]="20E6";
char freq_steps[10]="1201";

struct timeval t0,t1,t2,t3;
unsigned long elapsed;

int mlog_data_command(char *command,double *array[FREQS],int b) {
  int count,rval,sample_count;
  char output[10]="";
  char command2[80];
  char cmd_str[80],prompt_str[10],data_str[1000];
  int cr,lf;
      strcpy(command2,command);
      if (verbose>2) printf("%d Command: %s\n",strlen(command2),command2);
      write(sock, &command2, sizeof(char)*strlen(command2));
      cr=0;
      lf=0;
      count=0;
      if (verbose>2) fprintf(stdout,"Command Output String::\n");
      strcpy(cmd_str,"");
      while((cr==0) || (lf==0)){
        rval=read(sock, &output, sizeof(char)*1);
#ifdef __QNX__
        if (rval<1) delay(100);
#else
        if (rval<1) {
          usleep(10);
  
        }
#endif
        if (output[0]==13) {
          cr++;
          continue;
        }
        if (output[0]==10) {
          lf++;
          continue;
        }
        count+=rval;
        strncat(cmd_str,output,rval);
        if (verbose>2) fprintf(stdout,"%c",output[0]);
      }
      if (verbose>2) printf("Processing Data\n");

      cr=0;
      lf=0;
      count=0;
      sample_count=0;
      if (verbose>2) fprintf(stdout,"\nData Output String::\n");
      strcpy(data_str,"");
      if (verbose>2) fprintf(stdout,"%d: ",sample_count);
      while((cr==0) || (lf==0)){
        rval=read(sock, &output, sizeof(char)*1);
        if (output[0]==13) {
          cr++;
          continue;
        }
        if (output[0]==10) {
          lf++;
          continue;
        }
        if(output[0]==',') {
             if((sample_count % 2) == 0) {
               if (sample_count/2 >=FREQS) {
                 printf("ERROR: too many samples... aborting\n");
                 exit(-1);
               }
               array[sample_count/2][b]=atof(data_str);
               if (verbose>2) fprintf(stdout,"%s  ::  %lf",data_str,array[sample_count/2][b]);
             }
             sample_count++;
             if (verbose>2) fprintf(stdout,"\n%d: ",sample_count);
             strcpy(data_str,"");
        } else {
             strncat(data_str,output,rval);
        }
      }
      if((sample_count % 2) == 0) {
        if (sample_count/2 >=FREQS) {
          printf("ERROR: too many samples... aborting\n");
          exit(-1);
        }
        array[sample_count/2][b]=atof(data_str);
        if (verbose>2) fprintf(stdout,"%s  ::  %lf",data_str,array[sample_count/2][b]);
      }
      sample_count++;
      strcpy(data_str,"");
      if (verbose>2) fprintf(stdout,"\nSamples: %d\n",sample_count/2);
      if (verbose>2) fprintf(stdout,"\nPrompt String::\n");
      while(output[0]!='>'){
        rval=read(sock, &output, sizeof(char)*1);
#ifdef __QNX__
        if (rval<1) delay(100);
#else
        if (rval<1) usleep(10);
#endif
        strncat(prompt_str,output,rval);
        if (verbose>2) fprintf(stdout,"%c",output[0]);
      }
  return 0;
}

int button_command(char *command) {
  int count,rval;
  char output[10]="";
  char command2[80];
  char prompt_str[80];
/*
*  Process Command String with No feedback 
*/
      strcpy(command2,command);
      if (verbose>2) fprintf(stdout,"%d Command: %s\n",strlen(command2),command2);
      write(sock, &command2, sizeof(char)*strlen(command2));
      count=0;
      if (verbose>2) fprintf(stdout,"\nPrompt String::\n");
      while(output[0]!='>'){
        rval=read(sock, &output, sizeof(char)*1);
        strncat(prompt_str,output,rval);
        if (verbose>2) fprintf(stdout,"%c",output[0]);
        count++;
      }
      if (verbose>2) fprintf(stdout,"Command is done\n",command2);
      fflush(stdout);
  return 0;
}
void mypause ( void ) 
{ 
  fflush ( stdin );
  printf ( "Press [Enter] to continue . . ." );
  fflush ( stdout );
  getchar();
} 
/*-SET WRITE ENABLE BIT-------------------------------------------------------*/
int set_WE(int base,int onoff){
        int temp;
        if(onoff==OFF){
                temp=in8(base+PC_GRP_0);
                out8(base+PC_GRP_0,temp & 0xfe);
        }
        if(onoff==ON){
                temp=in8(base+PC_GRP_0);
                out8(base+PC_GRP_0,temp | 0x01);
        }
}
/*-SET READ/WRITE BIT-------------------------------------------------------*/
int set_RW(int base,int rw){
        int temp;
        if(rw==READ){
                temp=in8(base+PC_GRP_0);
                out8(base+PC_GRP_0,temp & 0xbf);
        }
        if(rw==WRITE){
                temp=in8(base+PC_GRP_0);
                out8(base+PC_GRP_0,temp | 0x40);
        }
}
/*-SET SWITCHED/ATTEN BIT-------------------------------------------------------*/
int set_SA(int base,int sa){
        int temp;
        if(sa==SWITCHES){
                temp=in8(base+PC_GRP_0);
                out8(base+PC_GRP_0,temp & 0x7f);
        }
        if(sa==ATTEN){
                temp=in8(base+PC_GRP_0);
                out8(base+PC_GRP_0,temp | 0x80);
        }
}


/*-REVERSE_BITS-------------------------------------------------------*/
int reverse_bits(int data){
	
	int temp=0;
	
	temp=temp + ((data & 1)  << 12);
	temp=temp + ((data & 2)  << 10);
	temp=temp + ((data & 4)  << 8);
	temp=temp + ((data & 8)  << 6);
	temp=temp + ((data & 16)  << 4);
	temp=temp + ((data & 32)  << 2);
	temp=temp + ((data & 64)  << 0);
	temp=temp + ((data & 128)  >> 2);
	temp=temp + ((data & 256)  >> 4);
	temp=temp + ((data & 512)  >> 6);
	temp=temp + ((data & 1024)  >> 8);
	temp=temp + ((data & 2048)  >> 10);
	temp=temp + ((data & 4096)  >> 12);

	return temp;
}
/*-GET_DEALY---------------------------------------------------------*/
float get_delay(int code){
	
	int	i;
	float	delay;
	float	delaylist[]={0.25, 0.45, 0.8, 1.5, 2.75, 5, 8, 15, 25, 45, 80, 140, 250};

	delay=0;
	for(i=0;i<13;i++){
		delay+=( (code & (int)pow(2,i)) >> i)*delaylist[i];
	}	
	return delay;
}
/*-BEAM_CODE---------------------------------------------------------*/
int beam_code(unsigned int base, int code){
	/* the beam code is 13 bits, pAD0 thru pAD12.  This code
	   uses bits 0-7 of CH0, PortA, and bits 0-4 of CH0, PortB
	   to output the beam code. Note: The beam code is an address
	   of the EEPROMs in the phasing cards.  This code is broadcast
	   to ALL phasing cards.  If you are witing the EEPROM, then this
	   be the beam code you are writing
	*/
	
	int temp;
#ifdef __QNX__
    // bit reverse the code
	code=reverse_bits(code);

    // check if beam code is reasonable
	if ( (code>8192) | (code<0) ){
		fprintf(stderr,"INVALID BEAM CODE - must be between 0 and 8192\n");
		return -1;
	}
    // set CH0, Port A to lowest 8 bits of beam code and output on PortA
	temp=code & 0xff;
	out8(base+PA_GRP_0,temp);
    // set CH0, Port B to upper 5 bits of beam code and output on PortB
	temp=code & 0x1f00;
	temp=temp >> 8;
	out8(base+PB_GRP_0,temp);

    // verify that proper beam code was sent out
	temp=in8(base+PB_GRP_0);
	temp=(temp & 0x1f) << 8;
	temp=temp+in8(base+PA_GRP_0);
	if (temp==code) return 0;
	else{
		fprintf(stderr,"BEAM CODE OUTPUT ERROR - requested code not sent\n");
		return -1;
	}
#else
  return 0; 
#endif
}
/*-SELECT_CARD------------------------------------------------------*/
int select_card(unsigned int base, int address){

	/* This code selects a card to address.  This can be used for
	   writing data to the EEPROM, or to verify the output of the
	   EEPROM. There are 20 cards in the phasing matrix, addresses
	   0-19.  A card is addressed when this address corresponds to
	   the switches on the phasing card.  Card address 31 is reserved for
	   programming purposes.
	*/
#ifdef __QNX__	
	int temp;
	struct 	timespec nsleep;
	nsleep.tv_sec=0;
	nsleep.tv_nsec=5000;


    // check if card address is reasonable
	if ( (address>31) | (address<0) ){
		fprintf(stderr,"INVALID CARD ADDRESS - must be between 0 and 32\n");
		return -1;
	}
    // shift address left 1 bit (write enable is the lowest bit)
	address=address << 1;
    // mask out bits not used for addressing the cards
	address=address & 0x3e;
    // check for other bits in CH0, PortC that may be on
	temp=in8(base+PC_GRP_0);
	temp=temp & 0xc1;
    // add other bit of PortC to the address bits
	address=address+temp;
    // output the address and original other bits to PortC
	out8(base+PC_GRP_0,address);
	nanosleep(&nsleep,NULL);
    // verify the output
	temp=in8(base+PC_GRP_0);
	if (temp==address) return 0;
	else{
		fprintf(stderr,"CARD SELECT OUTPUT ERROR - requested code not sent\n");
		fprintf(stderr," code=%d\n", temp);
		return -1;
	}
#else
  return 0;
#endif	
}

int write_attenuators(unsigned int base, int card, int code, int data){

        int temp;
        struct  timespec nsleep;
        nsleep.tv_sec=0;
        nsleep.tv_nsec=5000;

    // check that the data to write is valid
        if ( (data>63) | (data<0) ){
                fprintf(stderr,"INVALID DATA TO WRITE - must be between 0 and 63\n");
                return -1;
        }
        data=data ^ 0x3f;
    // select card to write
        temp=select_card(base,card);
    // choose the beam code to write (output appropriate EEPROM address
        temp=beam_code(base,code);
        set_SA(base,ATTEN);
    // enable writing
        set_RW(base,WRITE);
    // set CH1, PortA and Port B to output for writing
        out8(base+CNTRL_GRP_1,0x81);
    // bit reverse the data
        data=reverse_bits(data);
    // apply the data to be written to PortA and PortB on CH1
    // set CH1, Port A to lowest 8 bits of data and output on PortA
    // set CH1, Port A to lowest 8 bits of data and output on PortA
        temp=data & 0xff;
        out8(base+PA_GRP_1,temp);
    // set CH0, Port B to upper 5 bits of data and output on PortB
        temp=data & 0x1f00;
        temp=(temp >> 8);
        out8(base+PB_GRP_1,temp);
    // toggle write enable bit
        set_WE(base,ON);
        set_WE(base,OFF);
    // reset CH1, PortA and PortB to inputs
        out8(base+CNTRL_GRP_1,0x93);
    // disable writing
        set_RW(base,READ);
        delay(3);
    // verify written data
    // read PortA and PortB to see if EEPROM output is same as progammed
        temp=in8(base+PB_GRP_1);
        temp=temp & 0x1f;
        temp=temp << 8;
        temp=temp + in8(base+PA_GRP_1);
        temp=temp & 0x1f80;
        if (temp == data){
                //printf("    Code read after writing is %d\n", reverse_bits(temp));
         //       printf("       - DATA WAS WRITTEN: data: %x = readback: %x\n", reverse_bits(data), reverse_bits(temp));
                return 0;
        }
        else {
                printf(" ERROR - ATTEN DATA NOT WRITTEN: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
                return -1;
        }
}
/*-WRITE_CODE--------------------------------------------------------*/
int write_data_new(unsigned int base, int card, int code, int data){

        int temp;
        struct  timespec nsleep;
        nsleep.tv_sec=0;
        nsleep.tv_nsec=5000;


    // check that the data to write is valid
        if ( (data>8192) | (data<0) ){
                fprintf(stderr,"INVALID DATA TO WRITE - must be between 0 and 8192\n");
                return -1;
        }
        data=data ^ 0x1fff;
    // select card to write
        temp=select_card(base,card);
    // choose the beam code to write (output appropriate EEPROM address
        temp=beam_code(base,code);
        set_SA(base,SWITCHES);
    // enable writing
        set_RW(base,WRITE);
    // set CH1, PortA and Port B to output for writing
        out8(base+CNTRL_GRP_1,0x81);
    // bit reverse the data
        data=reverse_bits(data);
    // apply the data to be written to PortA and PortB on CH1
    // set CH1, Port A to lowest 8 bits of data and output on PortA
        temp=data & 0xff;
        out8(base+PA_GRP_1,temp);
    // set CH0, Port B to upper 5 bits of data and output on PortB
        temp=data & 0x1f00;
        temp=(temp >> 8);
        out8(base+PB_GRP_1,temp);
    // toggle write enable bit
        set_WE(base,ON);
        set_WE(base,OFF);
    // reset CH1, PortA and PortB to inputs
        out8(base+CNTRL_GRP_1,0x93);
    // disable writing
        set_RW(base,READ);
        delay(3);
    // verify written data
    // read PortA and PortB to see if EEPROM output is same as progammed
        temp=in8(base+PB_GRP_1);
        temp=temp & 0x1f;
        temp=temp << 8;
        temp=temp + in8(base+PA_GRP_1);
        temp=temp & 0x1fff;
        if (temp == data){
                //printf("    Code read after writing is %d\n", reverse_bits(temp));
                return 0;
        }
        else {
                printf(" ERROR - New Card DATA NOT WRITTEN: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
                return -1;
        }
}

/*-WRITE_CODE--------------------------------------------------------*/
int write_data_old(unsigned int base, int card, int code, int data){

        int temp;
        struct  timespec nsleep;
        nsleep.tv_sec=0;
        nsleep.tv_nsec=5000;

    // check that the data to write is valid
        if ( (data>8192) | (data<0) ){
                fprintf(stderr,"INVALID DATA TO WRITE - must be between 0 and 8192\n");
                return -1;
        }
    // select card 31 so that no real card is selected
        temp=select_card(base,31);
    // choose the beam code to write (output appropriate EEPROM address
        temp=beam_code(base,code);
    // enable writing (turn on WRITE_ENABLE);
        temp=in8(base+PC_GRP_0);
        temp=temp | 0x01;
        out8(base+PC_GRP_0,temp);
    // set CH1, PortA and Port B to output for writing
        out8(base+CNTRL_GRP_1,0x81);
    // bit reverse the data
        data=reverse_bits(data);
    // apply the data to be written to PortA and PortB on CH1
    // set CH1, Port A to lowest 8 bits of data and output on PortA
        temp=data & 0xff;
        out8(base+PA_GRP_1,temp);
    // set CH0, Port B to upper 5 bits of data and output on PortB
        temp=data & 0x1f00;
        temp=(temp >> 8);
        out8(base+PB_GRP_1,temp);
    // select card to write
        temp=select_card(base,card);
    // wait 20 ms to write
        //delay(20);
        //delay(1);
        //nanosleep(&nsleep,NULL);
    // select card 31 so that no real card is selected
        temp=select_card(base,31);
        //delay(5);
        delay(1);
        //nanosleep(&nsleep,NULL);
    // reset CH1, PortA and PortB to inputs
        out8(base+CNTRL_GRP_1,0x93);
    // disable writing (turn off WRITE_ENABLE);
        temp=in8(base+PC_GRP_0);
        temp=temp & 0xfe;
        out8(base+PC_GRP_0,temp);
        //delay(5);
        delay(1);
        //nanosleep(&nsleep,NULL);
    // verify written data
    // select card to read
        temp=select_card(base,card);
        //delay(5);
        delay(1);
        //nanosleep(&nsleep,NULL);
    // read PortA and PortB to see if EEPROM output is same as progammed
        temp=in8(base+PB_GRP_1);
        temp=temp & 0x1f;
        temp=temp << 8;
        temp=temp + in8(base+PA_GRP_1);
        if (temp == data){
                //printf("    Code read after writing is %d\n", reverse_bits(temp));
                temp=select_card(base,31);
                return 0;
        }
        else {
                printf(" ERROR - Old Card DATA NOT WRITTEN: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
                temp=select_card(base,31);
                return -1;
        }
}

/*-READ_DATA---------------------------------------------------------*/
int read_data(unsigned int base){
#ifdef __QNX__	
	int temp;

    // read PortA and PortB to see if EEPROM output is same as progammed
	//delay(1);
	temp=in8(base+PB_GRP_1);
	temp=temp & 0x1f;
	temp=temp << 8;
	temp=temp + in8(base+PA_GRP_1);

    // bit reverse data
	temp=reverse_bits(temp);

	return temp;
#else
  return 0;
#endif	
}


int main()
{
  char output[40],strout[40];
  char cmd_str[80],prompt_str[10],data_str[1000];
  double *phase[FREQS],*pwr_mag[FREQS];
  double freq[FREQS];
  int rval,count,sample_count,fail,cr,lf;
  int i=0,c=0,b=0; 
  int beamcode=0;
  double fstart;
  double fstop;
  double fstep;
  int fnum;

	int		 temp, pci_handle, j,  IRQ  ;
	unsigned char	 *BASE0, *BASE1;
	unsigned int	 mmap_io_ptr,IOBASE, CLOCK_RES;
	float		 time;
#ifdef __QNX__
	struct		 _clockperiod new, old;
	struct		 timespec start_p, stop_p, start, stop, nsleep;
    /* SET THE SYSTEM CLOCK RESOLUTION AND GET THE START TIME OF THIS PROCESS */
	new.nsec=10000;
	new.fract=0;
	temp=ClockPeriod(CLOCK_REALTIME,&new,0,0);
	if(temp==-1){
		perror("Unable to change system clock resolution");
	}
	temp=clock_gettime(CLOCK_REALTIME, &start_p);
	if(temp==-1){
		perror("Unable to read sytem time");
	}
	temp=ClockPeriod(CLOCK_REALTIME,0,&old,0);
	CLOCK_RES=old.nsec;
	printf("CLOCK_RES: %d\n", CLOCK_RES);
    /* OPEN THE PLX9656 AND GET LOCAL BASE ADDRESSES */
	fprintf(stderr,"PLX9052 CONFIGURATION ********************\n");
	clock_gettime(CLOCK_REALTIME, &start);
	temp=_open_PLX9052(&pci_handle, &mmap_io_ptr, &IRQ, 1);
	IOBASE=mmap_io_ptr;
	if(temp==-1){
		fprintf(stderr, "	PLX9052 configuration failed");
	}
	else{
		fprintf(stderr, "	PLX9052 configuration successful!\n");
	}
	printf("IOBASE=%x\n",IOBASE);
    /* INITIALIZE THE CARD FOR PROPER IO */
	// GROUP 0 - PortA=output, PortB=output, PortClo=output, PortChi=output
	out8(IOBASE+CNTRL_GRP_0,0x80);
	// GROUP 1 - PortAinput, PortB=input, PortClo=input, PortChi=output
	out8(IOBASE+CNTRL_GRP_1,0x93);
	out8(IOBASE+PA_GRP_0,0x00);
	out8(IOBASE+PB_GRP_0,0x00);
	out8(IOBASE+PC_GRP_0,0x00);
	out8(IOBASE+PA_GRP_1,0x00);
	out8(IOBASE+PB_GRP_1,0x00);
	temp=in8(IOBASE+PC_GRP_1);
	temp=temp & 0x0f;
	printf("input on group 1, port c is %x\n", temp);
#endif
  fnum=atoi(freq_steps);
  fstart=atof(freq_start);
  fstop=atof(freq_stop);
  fstep=(fstop-fstart)/(fnum-1);
  for(i=0;i<fnum;i++) {
    freq[i]=fstart+i*fstep;
    phase[i]=calloc(PHASECODES,sizeof(double));
    pwr_mag[i]=calloc(PHASECODES,sizeof(double));
  }

// Open Socket and initial IO
  if (verbose>0) printf("Opening Socket %s %d\n",hostip,port);
  sock=opentcpsock(hostip, port);
  if (sock < 0) {
    if (verbose>0) printf("Socket failure %d\n",sock);
  } else if (verbose>0) printf("Socket %d\n",sock);
  rval=read(sock, &output, sizeof(char)*10);
  if (verbose>0) fprintf(stdout,"Initial Output Length: %d\n",rval);
  strcpy(strout,"");
  strncat(strout,output,rval);
  if (verbose>0) fprintf(stdout,"Initial Output String: %s\n",strout);
  button_command(":SYST:PRES\r\n");
  sprintf(command,":SENS1:FREQ:STAR %s\r\n",freq_start);
  button_command(command);
  sprintf(command,":SENS1:FREQ:STOP %s\r\n",freq_stop);
  button_command(command);
  sprintf(command,":SENS1:SWE:POIN %s\r\n",freq_steps);
  button_command(command);
  button_command(":CALC1:PAR:COUN 2\r\n");
  button_command(":CALC1:PAR1:SEL\r\n");
  button_command(":CALC1:PAR1:DEF S21\r\n");
  button_command(":CALC1:FORM UPH\r\n");
  button_command(":CALC1:PAR2:SEL\r\n");
  button_command(":CALC1:PAR2:DEF S21\r\n");
  button_command(":CALC1:FORM MLOG\r\n");
  button_command(":SENS1:AVER ON\r\n");
  button_command(":SENS1:AVER:COUN 4\r\n");
  button_command(":SENS1:AVER:CLE\r\n");
  printf("\n\nCalibrate Network Analyzer for S12,S21\n");
  mypause();
  button_command(":SENS1:CORR:COLL:METH:THRU 1,2\r\n");
  sleep(1);
  button_command(":SENS1:CORR:COLL:THRU 1,2\r\n");
  printf("  Doing S1,2 Calibration..wait 4 seconds\n");
  sleep(4);

  button_command(":SENS1:CORR:COLL:METH:THRU 2,1\r\n");
  sleep(1);
  button_command(":SENS1:CORR:COLL:THRU 2,1\r\n");
  printf("  Doing S2,1 Calibration..wait 4 seconds\n");
  sleep(4);
  button_command(":SENS1:CORR:COLL:SAVE\r\n");
  printf("\n\nCalibration Complete\nReconfigure for Phasing Card Measurements");
  mypause();
  c=-1; 
  printf("\n\nEnter Radar Name: ");
  fflush(stdin);
  scanf("%s", &radar_name);
  fflush(stdout);
  fflush(stdin);
  printf("\n\nEnter Phasing Card Number: ");
  fflush(stdin);
  fflush(stdout);
  scanf("%d", &c);
  printf("Radar: <%s>  Card: %d\n",radar_name,c);
  fflush(stdout);
  while((c<CARDS) && (c >=0)) {
    sprintf(filename,"%s%s_%s_%d%s",dir,file_prefix,radar_name,c,file_ext);
    if (verbose>0) fprintf(stdout,"Using file: %s\n",filename);
    fflush(stdout);
    gettimeofday(&t0,NULL);
    calfile=fopen(filename,"w");
    count=PHASECODES;
    fwrite(&count,sizeof(int),1,calfile);
    count=CARDS;
    fwrite(&count,sizeof(int),1,calfile);
    count=fnum;
    fwrite(&count,sizeof(int),1,calfile);
    count=0;
    fwrite(freq,sizeof(double),fnum,calfile);
    if (verbose>0) {
      fprintf(stdout,"Writing beamcodes to phasing card\n");
      gettimeofday(&t2,NULL);
    }
    delay(10);
    for (b=0;b<PHASECODES;b++) {
      beamcode=b;
      if(NEW_PMAT) {
        temp=write_data_new(IOBASE,c,beamcode,b);
        temp=write_attenuators(IOBASE,c,beamcode,0);
      } else {
        temp=write_data_old(IOBASE,c,beamcode,b);
      }
    }
    if(verbose> 0 ) {
      gettimeofday(&t3,NULL);
      elapsed=(t3.tv_sec-t2.tv_sec)*1E6;
      elapsed+=(t3.tv_usec-t2.tv_usec);
      printf("  Elapsed Seconds: %lf\n",(float)elapsed/1E6);
    }
    if (verbose>0) fprintf(stdout,"Measuring phases\n");
    for (b=0;b<PHASECODES;b++) {
// Start of Command Block
      if(verbose> 0 ) {
        printf(":::Card %d::: PhaseCode %d\n",c,b);
        gettimeofday(&t2,NULL);
      }

//write phasecode to card.
      beamcode=b;
//select beamcode
      temp=select_card(IOBASE,c);	
      beam_code(IOBASE,beamcode);
      button_command(":CALC1:PAR1:SEL\r\n");
      button_command(":SENS1:AVER:CLE\r\n");
#ifdef __QNX__
      delay(400);
#else
      usleep(40000);
#endif
      mlog_data_command(":CALC1:DATA:FDAT?\r\n",phase,b) ;
      button_command(":CALC1:PAR2:SEL\r\n");
      mlog_data_command(":CALC1:DATA:FDAT?\r\n",pwr_mag,b) ;
//      single_data_command(":CALC1:DATA:FDAT?\r\n",freqs,b) ;
      if(verbose> 0 ) {
        gettimeofday(&t3,NULL);
        elapsed=(t3.tv_sec-t2.tv_sec)*1E6;
        elapsed+=(t3.tv_usec-t2.tv_usec);
        printf("  Elapsed Seconds: %lf\n",(float)elapsed/1E6);
      }

    } // end of phasecode loop
    for(i=0;i<fnum;i++) {
      if (verbose > 1) printf("Freq %lf:  Phase 0:%lf Phase 8191: %lf\n",freq[i],phase[i][0],phase[i][PHASECODES-1]);
      fwrite(&i,sizeof(int),1,calfile);
      count=fwrite(phase[i],sizeof(double),PHASECODES,calfile);
      count=fwrite(pwr_mag[i],sizeof(double),PHASECODES,calfile);
//      printf("Freq index: %d Count: %d\n",i,count);
    }
    printf("Closing File\n");
    fclose(calfile);
    if(verbose> 0 ) {
      gettimeofday(&t1,NULL);
      elapsed=(t1.tv_sec-t0.tv_sec)*1E6;
      elapsed+=(t1.tv_usec-t0.tv_usec);
      printf("%d Cards %d PhaseCodes, Elapsed Seconds: %lf\n",CARDS,PHASECODES,(float)elapsed/1E6);
    }
    c=-1;
//    if (verbose>0) fprintf(stdout,"Asking for another card:\n");
//    printf("Enter Next Card Number (CTRL-C to exit): ");
//    scanf("%d", &c);
  } // end of Card loop
}

