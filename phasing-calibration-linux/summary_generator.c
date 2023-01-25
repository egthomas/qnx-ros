#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define MAX_CARDS 20
#define MAX_FREQS 1201
#define MAX_PHASES 8192
#define MAX_ANGLES 16
int verbose=0;
char radar_name[80]="kansas_tx_west";
char dirstub[80]="/home/radar/calibrations/kansas_tx_west/";
FILE *timedelayfile=NULL;
FILE *summaryfile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;



int main()
{
  double *freq,*pwr_mag[MAX_FREQS],*timedelay[MAX_FREQS];    
  int best_phasecode[MAX_FREQS][MAX_ANGLES];
  int bad_card[MAX_CARDS];
  int b,c,i,ii,p,count;
  int summary_freqs,summary_phases;
  int num_freqs,num_phasecodes,num_cards,num_angles;
  char filename[120];
  int highest_time0_card[MAX_FREQS]; // card with highest time0 delay
  double highest_time0_value[MAX_FREQS]; // highest time0 delay in ns
  int lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  double lowest_pwr_mag=1E10; // freq,card,phasecode
//  int lowest_pwr_card[MAX_FREQS][MAX_PHASES]; // card with highest time0 delay
//  double lowest_pwr_value[MAX_FREQS][MAX_PHASES]; // highest time0 delay in ns
  double time_needed,angle,difference;
  if (verbose>1) printf("Nulling arrays\n");
  freq=NULL;
  num_freqs=0;
  for(i=0;i<MAX_FREQS;i++) {
        timedelay[i]=NULL;
        pwr_mag[i]=NULL;
        highest_time0_value[i]=-1000;
        highest_time0_card[i]=-1;
  }

  for(c=0;c<MAX_CARDS;c++) {
    sprintf(filename,"%s/timedelay_cal_%s_%d.dat",dirstub,radar_name,c);
    timedelayfile=fopen(filename,"r");
    printf("%p %s\n",timedelayfile,filename); 
    if (timedelayfile!=NULL) {
      fread(&num_phasecodes,sizeof(int),1,timedelayfile);
      if (verbose>1) printf("PhaseCodes: %d\n",num_phasecodes);
      if (num_phasecodes != MAX_PHASES) fprintf(stderr,"FILE: %s  ERROR:  Wrong number of phases  %d %d\n",filename,num_phasecodes,MAX_PHASES);
      fread(&num_cards,sizeof(int),1,timedelayfile);
      if (verbose>1) printf("Cards: %d\n",num_cards);
      if (num_cards != MAX_CARDS) fprintf(stderr,"FILE: %s  ERROR:  Wrong number of cards  %d %d\n",filename,num_cards,MAX_CARDS);
      fread(&num_freqs,sizeof(int),1,timedelayfile);
      if (verbose>1) printf("Freqs: %d\n",num_freqs);
      if (num_freqs != MAX_FREQS) fprintf(stderr,"FILE: %s  ERROR:  Wrong number of freqs  %d %d\n",filename,num_freqs,MAX_FREQS);
      if (num_freqs>MAX_FREQS) {
        fprintf(stderr,"Too many stored frequencies...up the MAX_FREQS define!\n");
        exit(0);
      }

      if (verbose>1) printf("Allocating arrays\n");
      for(i=0;i<num_freqs;i++) {
        if (freq!=NULL) free(freq);
        freq=calloc(num_freqs,sizeof(double));
        if (timedelay[i]!=NULL) free(timedelay[i]);
        timedelay[i]=calloc(num_phasecodes,sizeof(double));
        if (pwr_mag[i]!=NULL) free(pwr_mag[i]);
        pwr_mag[i]=calloc(num_phasecodes,sizeof(double));
      }


      if (verbose>1) printf("Reading frequency array\n");

      count=fread(freq,sizeof(double),num_freqs,timedelayfile);
      if (verbose>1) printf("%d %d\n",num_freqs,count);
      count=1;
      if (verbose>1) printf("Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int),1,timedelayfile);
          if (count==0) {
            break;
          }
          count=fread(timedelay[ii],sizeof(double),num_phasecodes,timedelayfile);
//          printf("Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(pwr_mag[ii],sizeof(double),num_phasecodes,timedelayfile);
//          printf("Freq index: %d Pwr-mag Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
      }
      if (count==0) {
        if (feof(timedelayfile)) if (verbose>1) printf("End of File!\n");
      }
      fclose(timedelayfile);
      bad_card[c]=0;
      for(i=0;i<num_freqs;i++) {
        if (timedelay[i][0]>highest_time0_value[i]) {
            highest_time0_card[i]=c;
            highest_time0_value[i]=timedelay[i][0];
        }        
        for (b=0;b<num_phasecodes;b++) {
          if (pwr_mag[i][b]< -20.0) bad_card[c]=1;
          if (pwr_mag[i][b]<lowest_pwr_mag) {
            lowest_pwr_mag_index[0]=i;
            lowest_pwr_mag_index[1]=c;
            lowest_pwr_mag_index[2]=b;
            lowest_pwr_mag=pwr_mag[i][b];
          }        
        }
      }


    }  //timedelay file if
  } //card loop
//Summary stats
  printf("::: Summary Stats for All Cards :::\n");
  for(i=0;i<MAX_FREQS;i++) {
      if (highest_time0_card[i] >=0) {
        printf("%d :: %8.3lf  Highest 0-Time Delay: %8.3lf card: %d\n",i,freq[i],highest_time0_value[i],highest_time0_card[i]);
      }
  } //End of Freq Loop
  printf("Lowest Mag: %lf\n",lowest_pwr_mag);
  printf("Lowest Mag Index:: freq: %d card: %d phasecode: %d\n",
     lowest_pwr_mag_index[0],lowest_pwr_mag_index[1],lowest_pwr_mag_index[2]);
  for (c=0;c<MAX_CARDS;c++) {
    if (bad_card[c]) printf("Card %d has low pwr\n",c);
  }
  sprintf(filename,"%s/timedelay_summary_%s.dat",dirstub,radar_name);
  summaryfile=fopen(filename,"w+");
  if (verbose > 1 ) printf("Creating %p %s\n",summaryfile,filename); 
  if (summaryfile!=NULL) {
    summary_freqs=MAX_FREQS;
    fwrite(&summary_freqs,sizeof(int),1,summaryfile);
    fwrite(highest_time0_card,sizeof(int),summary_freqs,summaryfile);
    fwrite(highest_time0_value,sizeof(double),summary_freqs,summaryfile);
    fwrite(lowest_pwr_mag_index,sizeof(int),3,summaryfile);
    fwrite(&lowest_pwr_mag,sizeof(double),1,summaryfile);
    printf("Lowest Mag Index:: freq: %d card: %d phasecode: %d\n",
      lowest_pwr_mag_index[0],lowest_pwr_mag_index[1],lowest_pwr_mag_index[2]);
    printf("Lowest Mag: %lf\n",lowest_pwr_mag);
    fclose(summaryfile);
  }


} //main
