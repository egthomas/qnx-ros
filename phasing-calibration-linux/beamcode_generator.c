#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define MAX_CARDS 20
#define MAX_FREQS 1500
#define MAX_PHASES 8192
#define MAX_STD_ANGLES 22 
#define MAX_ANGLES 101 
int verbose=1;
char radar_name[80]="kansas_tx_west";
char dirstub[80]="/superdarn/calibrations/kansas_tx_west/";
FILE *timedelayfile=NULL;
FILE *summaryfile=NULL;
FILE *beamcodefile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;
double standard_angles[MAX_STD_ANGLES],angles[MAX_ANGLES];
int antenna_best_code[MAX_CARDS];
double spacing=12.8016; //meters 
double bm_sep=3.24;
double middle=10.5; //(MAX_STD_ANGLES-1)/2

double timedelay_needed(double angle,double spacing,int card) {
/*
*  angle from broadside (degrees)  spacing in meters
*/
  double deltat=0;
  double needed=0;
  double c=0.299792458; // meters per nanosecond
  int antenna=-1;
  double radians=0.0;
  if (card > 15) antenna=card-10;
  else antenna=card;
  deltat=(spacing/c)*sin((fabs(angle)*3.14159)/180.0); //nanoseconds
  if (angle > 0) needed=antenna*deltat;
  if (angle < 0) needed=(15-antenna)*deltat;
  if (needed < 0) {
    printf("Error in Time Needed Calc: %lf %lf\n",needed,deltat);
  }
  return needed; //nanoseconds
}

int main()
{
  double *freq,*pwr_mag[MAX_FREQS],*timedelay[MAX_FREQS];    
  int best_phasecode[MAX_FREQS][MAX_ANGLES];
  int best_attencode[MAX_FREQS][MAX_ANGLES];
  double best_atten_value[MAX_FREQS][MAX_ANGLES];
  int b,c,i,ii,j,p,count;
  int summary_freqs,summary_phases;
  int num_freqs,num_phasecodes,num_cards,num_angles;
  char filename[120];
  int highest_time0_card[MAX_FREQS]; // card with highest time0 delay
  double highest_time0_value[MAX_FREQS]; // highest time0 delay in ns
  int lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  double lowest_pwr_mag=1E10,needed_atten=0.0; // freq,card,phasecode
//  int lowest_pwr_card[MAX_FREQS][MAX_PHASES]; // card with highest time0 delay
//  double lowest_pwr_value[MAX_FREQS][MAX_PHASES]; // highest time0 delay in ns
  double time_needed,angle,difference;
  double atten,atten_steps[6];
  int best_atten_code;
  printf("Nulling arrays\n");
  freq=NULL;
  num_freqs=0;
  for(i=0;i<MAX_FREQS;i++) {
        timedelay[i]=NULL;
        pwr_mag[i]=NULL;
        highest_time0_value[i]=0;
        highest_time0_card[i]=-1;
  }
  for(i=0;i<6;i++) {
    atten_steps[i]=0.5*pow(2,i);
  }

  for(i=0;i<MAX_STD_ANGLES;i++) {
    standard_angles[i]=(i-middle)*bm_sep;
    printf("%d %lf\n",i,standard_angles[i]);
  }
  for(i=0;i<MAX_ANGLES;i++) {
    angles[i]=-MAX_ANGLES/2+i;
    printf("%d %lf\n",i,angles[i]);
  }

  sprintf(filename,"%s/timedelay_summary_%s.dat",dirstub,radar_name,c);
  summaryfile=fopen(filename,"r");
  printf("%p %s\n",summaryfile,filename); 
  if (summaryfile!=NULL) {
    fread(&summary_freqs,sizeof(int),1,summaryfile);
    fread(highest_time0_card,sizeof(int),summary_freqs,summaryfile);
    fread(highest_time0_value,sizeof(double),summary_freqs,summaryfile);
    fread(lowest_pwr_mag_index,sizeof(int),3,summaryfile);
    fread(&lowest_pwr_mag,sizeof(double),1,summaryfile);
    printf("Summary Lowest Mag Index:: freq: %d card: %d phasecode: %d\n",
      lowest_pwr_mag_index[0],lowest_pwr_mag_index[1],lowest_pwr_mag_index[2]);
    printf("Summary: lowest_pwr_mag %lf\n",lowest_pwr_mag);
    fclose(summaryfile);
  }
  for(c=0;c<MAX_CARDS;c++) {
    sprintf(filename,"%s/timedelay_cal_%s_%d.dat",dirstub,radar_name,c);
    timedelayfile=fopen(filename,"r");
    printf("%p %s\n",timedelayfile,filename); 
    if (timedelayfile!=NULL) {
      fread(&num_phasecodes,sizeof(int),1,timedelayfile);
      printf("PhaseCodes: %d\n",num_phasecodes);
      fread(&num_cards,sizeof(int),1,timedelayfile);
      printf("Cards: %d\n",num_cards);
      fread(&num_freqs,sizeof(int),1,timedelayfile);
      printf("Freqs: %d\n",num_freqs);
      if (num_freqs>MAX_FREQS) {
        printf("Too many stored frequencies...up the MAX_FREQS define!\n");
        exit(0);
      }

      printf("Allocating arrays\n");
      for(i=0;i<num_freqs;i++) {
        if (freq!=NULL) free(freq);
        freq=calloc(num_freqs,sizeof(double));
        if (timedelay[i]!=NULL) free(timedelay[i]);
        timedelay[i]=calloc(num_phasecodes,sizeof(double));
        if (pwr_mag[i]!=NULL) free(pwr_mag[i]);
        pwr_mag[i]=calloc(num_phasecodes,sizeof(double));
      }


      printf("Reading frequency array\n");

      count=fread(freq,sizeof(double),num_freqs,timedelayfile);
      printf("%d %d\n",num_freqs,count);
      count=1;
      printf("Reading in data\n");
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
        if (feof(timedelayfile)) printf("End of File!\n");
      }
      fclose(timedelayfile);
      sprintf(filename,"%s/beamcodes_cal_%s_%d.dat",dirstub,radar_name,c);
      beamcodefile=fopen(filename,"w+");
      printf("%p %s\n",beamcodefile,filename); 
      if (beamcodefile!=NULL) {
        fwrite(&num_freqs,sizeof(int),1,beamcodefile);
        fwrite(freq,sizeof(double),num_freqs,beamcodefile);
        num_angles=MAX_STD_ANGLES;
        fwrite(&num_angles,sizeof(int),1,beamcodefile);
        fwrite(standard_angles,sizeof(double),num_angles,beamcodefile);
        for(i=0;i<num_freqs;i++) {
          for(b=0;b<num_angles;b++) {
            angle=standard_angles[b];
            time_needed=timedelay_needed(angle,spacing,c)+highest_time0_value[i]; 
            difference=1000;
            best_phasecode[i][b]=-1;
            for(p=0;p<num_phasecodes;p++) {
              if(fabs(time_needed-timedelay[i][p])<difference) {
                best_phasecode[i][b]=p;
                difference=fabs(time_needed-timedelay[i][p]);
              }
            } //phase code looop
            needed_atten=pwr_mag[i][best_phasecode[i][b]]-lowest_pwr_mag;
            best_attencode[i][b]=0;
            best_atten_value[i][b]=0.0;
            for(ii=0;ii<64;ii++) {
              atten=0;
              for(j=0;j<6;j++) {
                if((ii & (int)pow(2,j))==pow(2,j)) {
                  atten+=atten_steps[j];
                }
              }
              if((atten-needed_atten) < 0) {
                if(fabs(atten-needed_atten) < fabs(best_atten_value[i][b]-needed_atten)) {
                  best_attencode[i][b]=ii;
                  best_atten_value[i][b]=atten;
                }
              }
            }

            //if(i==0) printf("Freq: %d Angle: %lf  Card: %d  Code: %d Needed Timedelay: %lf Best Timedelay: %lf\n",i,angle,c,best_phasecode[i][b],time_needed,timedelay[i][best_phasecode[i][b]]);  
            if(i==0) printf("Freq: %d Angle: %lf  Card: %d  Code: %d lo_P: %lf P: %lf N_A: %lf B_A: %lf B_Acode: %d\n",
                            i,angle,c,best_phasecode[i][b],lowest_pwr_mag,pwr_mag[i][best_phasecode[i][b]],
                            needed_atten,best_atten_value[i][b],best_attencode[i][b]);  
          } //beam direction loop
          fwrite(best_phasecode[i],sizeof(int),num_angles,beamcodefile);
          fwrite(best_attencode[i],sizeof(int),num_angles,beamcodefile);
        } // frequency loop

        num_angles=MAX_ANGLES;
        fwrite(&num_angles,sizeof(int),1,beamcodefile);
        fwrite(angles,sizeof(double),num_angles,beamcodefile);
        for(i=0;i<num_freqs;i++) {
          for(b=0;b<num_angles;b++) {
            time_needed=timedelay_needed(angle,spacing,c)+highest_time0_value[i];
            difference=1000;
            best_phasecode[i][b]=-1;
            angle=angles[b];
            for(p=0;p<num_phasecodes;p++) {
              if(fabs(time_needed-timedelay[i][p])<difference) {
                best_phasecode[i][b]=p;
                difference=fabs(time_needed-timedelay[i][p]);
              }
            } //phase code looop
            needed_atten=pwr_mag[i][best_phasecode[i][b]]-lowest_pwr_mag;
            best_attencode[i][b]=0;
            best_atten_value[i][b]=0.0;
            for(ii=0;ii<64;ii++) {
              atten=0;
              for(j=0;j<6;j++) {
                if((ii & (int)pow(2,j))==pow(2,j)) {
                  atten+=atten_steps[j];
                }
              }
              if((atten-needed_atten) < 0) {
                if(fabs(atten-needed_atten) < fabs(best_atten_value[i][b]-needed_atten)) {
                  best_attencode[i][b]=ii;
                  best_atten_value[i][b]=atten;
                }
              }
            }

            //printf("Freq: %d Angle: %lf  Card: %d  Code: %d Timedelay: %lf\n",i,angle,c,best_phasecode[i][b],time_needed);
          } //beam direction loop
          fwrite(best_phasecode[i],sizeof(int),num_angles,beamcodefile);
          fwrite(best_attencode[i],sizeof(int),num_angles,beamcodefile);
        } // frequency loop

        fclose(beamcodefile);
      }  //beamcode file if
    }  //timedelay file if

// free card arrays
  } //card loop
} //main
