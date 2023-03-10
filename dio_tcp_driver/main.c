#include <sys/types.h> 
#ifdef __QNX__
  #include <hw/inout.h>
  #include <sys/socket.h>
  #include <sys/neutrino.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
#endif
#include <signal.h>
#include <math.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "control_program.h"
#include "global_server_variables.h"
#include "priority.h"
#include "include/plx_functions.h"
#include "include/plx_defines.h"
#include "utils.h"
#include "rtypes.h"
#define IMAGING 0

int sock,msgsock;
int verbose=0;
int configured=1;
uint32_t use_beam_table;
int32_t num_freqs[MAX_RADARS],num_beamcodes[MAX_RADARS],num_fsteps[MAX_RADARS],foffset[MAX_RADARS];
double *f_lo[MAX_RADARS],*f_hi[MAX_RADARS],*f_c[MAX_RADARS];
int32_t *f_bmnum[MAX_RADARS];
 
void graceful_cleanup(int signum)
{
  char *diohostip=DIO_HOST_IP;
  int dioport=DIO_HOST_PORT;
  close(msgsock);
  close(sock);
  exit(0);
}
static unsigned char reverse_addr(unsigned char x) {
  unsigned char h; 
  int i=0;
  for(h=i=0; i< 5; i++) {
    h=( h << 1 ) + (x & 1);
    x>>=1;
  }
  return h;
}

int main(){

    // DECLARE AND INITIALIZE ANY NECESSARY VARIABLES
	// socket and message passing variables
	int	data;
        unsigned char if_addr,rf_addr,addr,card;
        uint32_t ifmode=IF_ENABLED;
        char   radar_table_1[256];
        char   radar_table_2[256];
        struct RXFESettings rf_settings;
        struct RXFESettings if_settings;
	char *diohostip=DIO_HOST_IP;
	int dioport=DIO_HOST_PORT;

	int	rval;
        char    datacode;
        fd_set rfds,efds;

	// function specific message variables
        struct DriverMsg msg;
        int     maxclients=MAX_RADARS*MAX_CHANNELS;
        struct  ControlPRM  clients[maxclients],client ;
        struct tx_status txstatus;

	// counter and temporary variables
	int	a,b,i,j,k,r,c,buf,tx;
        int     numclients=0;
        int     best_client;
	int 	temp;
        int  ready_index[MAX_RADARS][MAX_CHANNELS];
	// pci, io, and memory variables
	unsigned int	 mmap_io_ptr,IOBASE;
	int		pci_handle,IRQ;
	// timing related variables
        struct timeval tv;
	struct	timespec	start, stop, sleep;
        FILE *beamtablefile;
        char filename[256];
        char dir[20]=BEAMTABLE_DIR;
#ifdef __QNX__       
	struct	 _clockperiod 	new, old;
	int	clockresolution;
#endif

        signal(SIGINT, graceful_cleanup);
        use_beam_table=0;
        for (r=0;r<MAX_RADARS;r++){
          for (c=0;c<MAX_CHANNELS;c++){
            ready_index[r][c]=-1;
          }
        }
    // setup default rxfe settings.
       rf_settings.amp1=1;  
       rf_settings.amp2=0;  
       rf_settings.amp3=0;
       rf_settings.att1=0;
       rf_settings.att2=0;
       rf_settings.att3=0;
       rf_settings.att4=0;
       rf_settings.ifmode=0;
       rf_addr=build_RXFE_EEPROM_address(rf_settings);
       if_settings.amp1=1;  
       if_settings.amp2=1;  
       if_settings.amp3=1;
       if_settings.att1=0;
       if_settings.att2=0;
       if_settings.att3=0;
       if_settings.att4=0;
       if_settings.ifmode=1;
       if_addr=build_RXFE_EEPROM_address(if_settings);  
#ifdef __QNX__       
    // SET THE SYSTEM CLOCK RESOLUTION AND GET THE START TIME OF THIS PROCESS 
	// set the system clock resolution to 10 us

/*
	new.nsec=10000;
	new.fract=0;
	temp=ClockPeriod(CLOCK_REALTIME,&new,0,0);
	if(temp==-1) 	perror("Unable to change system clock resolution");
*/
	temp=ClockPeriod(CLOCK_REALTIME,0,&old,0);
	if(temp==-1) 	perror("Unable to read sytem time");
	clockresolution=old.nsec;
	if (verbose > 1) printf("System clock resolution is %d ns\n",clockresolution);

        for (i=0;i<MAX_TRANSMITTERS;i++) {
          txstatus.status[i]=0xf;
          txstatus.AGC[i]=1;
          txstatus.LOWPWR[i]=1;
        }

	i=0;

        if(configured) {
      // OPEN THE PLX9052 AND GET LOCAL BASE ADDRESSES 
	  temp=_open_PLX9052(&pci_handle, &mmap_io_ptr, &IRQ, 1);
          IOBASE=mmap_io_ptr;
          if(temp==-1){
                if (verbose > 1) fprintf(stderr, "       PLX9052 configuration failed");
          }
          else{
                if (verbose > 1) fprintf(stderr, "       PLX9052 configuration successful!\n");
          }
    // INITIALIZE DIO BOARD
          // GROUP 0 - PortA=output, PortB=output, PortClo=output, PortChi=output
          out8(IOBASE+CNTRL_GRP_0,0x80);
          // GROUP 1 - PortAinput, PortB=input, PortClo=input, PortChi=output
          out8(IOBASE+CNTRL_GRP_1,0x93);
          out8(IOBASE+PA_GRP_0,0x00);
          out8(IOBASE+PB_GRP_0,0x00);
          out8(IOBASE+PC_GRP_0,0x00);
          out8(IOBASE+PA_GRP_1,0x00);
          out8(IOBASE+PB_GRP_1,0x00);
          if (DEVICE_ID==0x0c78) {
          // GROUP 2 - PortA=output, PortB=output, PortClo=output, PortChi=output
            out8(IOBASE+CNTRL_GRP_2,0x80);
          // GROUP 3 - PortAinput, PortB=input, PortClo=input, PortChi=output
            out8(IOBASE+CNTRL_GRP_3,0x93);
            out8(IOBASE+PA_GRP_2,0x00);
            out8(IOBASE+PB_GRP_2,0x00);
            out8(IOBASE+PC_GRP_2,0x00);
            out8(IOBASE+PA_GRP_3,0x00);
            out8(IOBASE+PB_GRP_3,0x00);
            out8(IOBASE+CNTRL_GRP_4,0x80);
            out8(IOBASE+PC_GRP_4,0x00);
            out8(IOBASE+PA_GRP_4,0x00);
            out8(IOBASE+PB_GRP_4,0xff);
          }
        } else {
          printf("Not configured\n");
        }
        ifmode=IF_ENABLED;
        if(ifmode) {
          printf("IF ENABLED\n"); 
          set_RXFE_EEPROM_address(IOBASE, if_addr);
        } else {
          printf("RF ENABLED \n"); 
          set_RXFE_EEPROM_address(IOBASE, rf_addr);
        }
//        for(i=0;i<MAX_RADARS;i++) {                                                         
//          for(j=0;j<2;j++) {                                                                
//            card=reverse_addr(i*2+j);                                                       
//            temp=read_RXFE_EEPROM_address(IOBASE, card, 0); 
//          }
//        }
        //set_RXFE_EEPROM_address(IOBASE,0); 
#endif
    // OPEN TCP SOCKET AND START ACCEPTING CONNECTIONS 
	sock=tcpsocket(DIO_HOST_PORT);
//	sock=server_unixsocket("rosdio",0);

	listen(sock, 5);

	while(1){
                rval=1;
		msgsock=accept(sock, 0, 0);
		if (verbose > 0) printf("accepting socket!!!!!\n");
		if( (msgsock==-1) ){
			perror("accept FAILED!");
			return EXIT_FAILURE;
		}
		else while (rval>=0){
                  FD_ZERO(&rfds);
                  FD_SET(msgsock, &rfds); //Add msgsock to the read watch
                  FD_ZERO(&efds);
                  FD_SET(msgsock, &efds);  //Add msgsock to the exception watch
                  /* Wait up to five seconds. */
                  tv.tv_sec = 5;
                  tv.tv_usec = 0;
		  if (verbose > 0) printf("%d Entering Select\n",msgsock);
                  rval = select(msgsock+1, &rfds, NULL, &efds, NULL);
		  if (verbose > 0) printf("%d Leaving Select %d\n",msgsock,rval);
                  /* Don???t rely on the value of tv now! */
                  if (FD_ISSET(msgsock,&efds)){
                    if (verbose > 0) printf("Exception on msgsock %d ...closing\n",msgsock);
                    break;
                  }
                  if (rval == -1) perror("select()");
                  rval=recv(msgsock, &buf, sizeof(int), MSG_PEEK); 
                  if (verbose>0) printf("%d PEEK Recv Msg %d\n",msgsock,rval);
		  if (rval==0) {
                    if (verbose > 0) printf("Remote Msgsock %d client disconnected ...closing\n",msgsock);
                    break;
                  } 
		  if (rval<0) {
                    if (verbose > 0) printf("Msgsock %d Error ...closing\n",msgsock);
                    break;
                  } 
                  if ( FD_ISSET(msgsock,&rfds) && rval>0 ) {
                    if (verbose>0) printf("Data is ready to be read\n");
		    if (verbose > 0) printf("%d Recv Msg\n",msgsock);
                    rval=recv_data(msgsock,&msg,sizeof(struct DriverMsg));
                    datacode=msg.type;
		    if (verbose > 0) printf("\nmsg code is %c\n", datacode);
				switch( datacode ){

					case DIO_CtrlProg_READY:
						if (verbose > 1) printf("\nAsking to set up dio info for client that is ready\n");	
						if (verbose > 1) printf("Read msg struct from tcp socket!\n");	
						rval=recv_data(msgsock,&client,sizeof(struct ControlPRM));
//                                                rval=recv_data(msgsock, &pulseseq, sizeof(struct TSGbuf));
                                                r=client.radar-1;
                                                c=client.channel-1;
                                                if ((ready_index[r][c]>=0) && (ready_index[r][c] <maxclients) ) {
                                                  clients[ready_index[r][c]]=client;
                                                } else {
                                                  clients[numclients]=client;
                                                  ready_index[r][c]=numclients;
                                                  numclients++;
                                                }
                                                numclients=numclients % maxclients;
						msg.status=1;
                                                rval=send_data(msgsock, &msg, sizeof(struct DriverMsg));
                                                break; 
					case DIO_GET_TX_STATUS:
						rval=recv_data(msgsock,&r,sizeof(r));
						if (verbose > 0) printf("\nDIO tx status for radar: %d\n",r);	
                                                if(configured) {  
                                                  msg.status=_get_status(IOBASE,r,&txstatus); 
                                                } else {
                                                }
						if (verbose > 0) {
                                                  for(tx=0;tx<16;tx++) {
                                                    if (verbose > 0 ) printf("  Transmitter: %d AGC: %d LOWPWR: %d\n",tx,txstatus.AGC[tx],txstatus.LOWPWR[tx]);	
                                                  }
                                                }
                                                rval=send_data(msgsock, &txstatus, sizeof(struct tx_status));
                                                rval=send_data(msgsock, &msg, sizeof(struct DriverMsg));
                                                break; 
					case DIO_PRETRIGGER:
						if (verbose > 1) printf("DIO Pretrigger\n");	
                                                if(configured) {  
                                                  if (IMAGING==0) {
                                                    if (TX_BEAM_PRIORITY==1) {
                                                  /*  priority beam direction*/
                                                      for (r=1;r<=MAX_RADARS;r++) {
                                                        best_client=maxclients+1; 
                                                        for (i=0;i<numclients;i++) {
                                                          if (clients[i].radar==r) { 
                                                            if (best_client >=maxclients) {
                                                              best_client=i;
                                                            } else {  
                                                              if (clients[i].priority<clients[best_client].priority) {
                                                                best_client=i;
                                                              }
                                                            }
                                                          }
                                                        }
                                                        if (best_client <maxclients) {
                                                          client=clients[best_client];
                                                          _select_card(IOBASE,&client); 
                                                          msg.status=_select_beam(IOBASE,&client); 
                                                        }
                                                      }
                                                    } else {
                                                  /*  Per radar channel phasing*/
                                                      for (i=0;i<numclients;i++) {
                                                        client=clients[best_client];
                                                        _select_card(IOBASE,&client); 
                                                        msg.status=_select_beam(IOBASE,&client); 
                                                      }
                                                    }
                                                  } else {
                                                  /* IMAGING DIO PRE-trigger steps*/
                                                  }
                                                }
                                                for (r=0;r<MAX_RADARS;r++){
                                                  for (c=0;c<MAX_CHANNELS;c++){
                                                    ready_index[r][c]=-1;
                                                  }
                                                }
                                                numclients=0;
						msg.status=1;
                                                rval=send_data(msgsock, &msg, sizeof(struct DriverMsg));
						if (verbose > 1) printf("\nDIO Pretrigger\n");	
                                                break; 
//					case GET_BEAM:
//						printf("\nAsking to get current beam number!\n");	
//						printf("Read tx msg struct from tcp socket!\n");	
//						initialize_status(&tx_status);
//						printf("Writing tx msg struct to tcp socket!\n");	
//						writetcp(msgsock,"b\n",sizeof("b\n"));
//						break;
					case DIO_CLRFREQ:
						if (verbose > 1) printf("DIO clrfreq setup\n");	
						rval=recv_data(msgsock,&client,sizeof(struct ControlPRM));
                                                _select_card(IOBASE,&client); 
                                                msg.status=_select_beam(IOBASE,&client); 
                                                // Set up RXFE for each radar
                                                if(ifmode) { 
                                                  set_RXFE_EEPROM_address(IOBASE, rf_addr);
                                                }
						msg.status=1;
                                                rval=send_data(msgsock, &msg, sizeof(struct DriverMsg));
						if (verbose > 1) printf("DIO clrfreq end\n");	
                                                break;
                                        case DIO_RXFE_RESET:
						if (verbose > 1) printf("\nDIO reset rxfe\n");	
                                                // Set up RXFE for each radar
                                                if(ifmode) { 
                                                  set_RXFE_EEPROM_address(IOBASE, if_addr);
                                                } else {
                                                  set_RXFE_EEPROM_address(IOBASE, rf_addr);
                                                } 
						msg.status=1;
                                                rval=send_data(msgsock, &msg, sizeof(struct DriverMsg));
                                                break;
                                        case DIO_RXFE_SETTINGS:
                                                if (verbose > 1) printf("DIO driver: Re-configuring RXFE Settings\n");
                                                rval=recv_data(msgsock,&ifmode,sizeof(ifmode)); 
                                                if (verbose > 1) printf("DIO driver: IF Mode %d \n",ifmode);
                                                rval=recv_data(msgsock,&rf_settings,sizeof(struct RXFESettings)); 
                                                rval=recv_data(msgsock,&if_settings,sizeof(struct RXFESettings));
                                                rval=send_data(msgsock, &msg, sizeof(struct DriverMsg));
                                                rf_addr=build_RXFE_EEPROM_address(rf_settings);
                                                if_addr=build_RXFE_EEPROM_address(if_settings);
                                                if(ifmode) {
                                                  if (verbose > 1) printf("IF ENABLED\n"); 
                                                  set_RXFE_EEPROM_address(IOBASE,if_addr); 
                                                } else {
                                                  if (verbose > 1) printf("RF ENABLED\n"); 
                                                  set_RXFE_EEPROM_address(IOBASE,rf_addr); 
                                                }
                                                break;
                                        case DIO_TABLE_SETTINGS:
                                                if (verbose > 1) fprintf(stdout,"DIO driver: Re-configuring Beam Table Settings\n");
                                                rval=recv_data(msgsock,&use_beam_table,sizeof(use_beam_table)); 
                                                rval=recv_data(msgsock,&radar_table_1,256*sizeof(char)); 
                                                rval=recv_data(msgsock,&radar_table_2,256*sizeof(char)); 
                                                rval=send_data(msgsock, &msg, sizeof(struct DriverMsg));
                                                if (verbose > 1) {
                                                  fprintf(stdout,"DIO driver: Use Table: %d\n",use_beam_table);
                                                  fprintf(stdout,"DIO driver: Table 1: %s\n",radar_table_1);
                                                  fprintf(stdout,"DIO driver: Table 2: %s\n",radar_table_2);
                                                }
						for(r=0;r<MAX_RADARS;r++) {
                                                  strcpy(filename,"");
                                                  if(r==0) strcpy(filename,radar_table_1);
                                                  if(r==1) strcpy(filename,radar_table_2);
                                                  fprintf(stdout,"Opening: %s\n",filename);
      						  beamtablefile=fopen(filename,"r");
      						  if(beamtablefile!=NULL) {
                                                        fprintf(stdout,"Opened: %p\n",beamtablefile);
						        fread(&num_beamcodes[r],sizeof(int32_t),1,beamtablefile);
						        fread(&foffset[r],sizeof(int32_t),1,beamtablefile);
						        fread(&num_fsteps[r],sizeof(int32_t),1,beamtablefile);
                                                        f_bmnum[r]=(int32_t *)malloc(sizeof(int32_t)*num_beamcodes[r]); 

                                                        f_c[r]=malloc(sizeof(double)*num_beamcodes[r]); 
                                                        f_lo[r]=malloc(sizeof(double)*num_beamcodes[r]); 
                                                        f_hi[r]=malloc(sizeof(double)*num_beamcodes[r]); 

						        fread(f_bmnum[r],sizeof(int32_t),num_beamcodes[r],beamtablefile);
						        fread(f_c[r],sizeof(double),num_beamcodes[r],beamtablefile);
						        fread(f_lo[r],sizeof(double),num_beamcodes[r],beamtablefile);
						        fread(f_hi[r],sizeof(double),num_beamcodes[r],beamtablefile);

                                                        fprintf(stdout,"Closing: %s\n",filename);
        						fclose(beamtablefile);
        						beamtablefile=NULL;
/*
                                                        for (b=0;b<num_beamcodes[r];b++) {
                                                          fprintf(stdout,"R: %d %d Code: %d Beam %d F_c: %e\n",
                                                            r,num_beamcodes[r],b,f_bmnum[r][b],f_c[r][b]);
                                                        } 
*/
      						  } else {
                                                        if(f_bmnum[r]!=NULL) free(f_bmnum[r]); 
                                                        if(f_c[r]!=NULL) free(f_c[r]); 
                                                        if(f_lo[r]!=NULL) free(f_lo[r]); 
                                                        if(f_hi[r]!=NULL) free(f_hi[r]); 
        						fprintf(stderr,"Error opening beam lookup table file\n");
						  }
						}
                                                break;
					default:
						if (verbose > 0) fprintf(stderr,"BAD CODE: %c : %d\n",datacode,datacode);
						break;

				}

			}
		} 
		close(msgsock);
		if (verbose > 0) fprintf(stderr,"Closing socket\n");
	}


        return 1;
}
