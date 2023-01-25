#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <gsl/gsl_fit.h>

#define MIN_CARD 19 
#define MAX_CARD 19 
#define MAX_CARDS 20 
#define MAX_FREQS 1500
#define MAX_PHASES 8192

int verbose=1;
char radar_name[80]="kansas_tx_west";
char dirstub[80]="/home/radar/calibrations/kansas_tx_west/";
FILE *calfile=NULL;
FILE *timedelayfile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;
double expected_timedelays[13]={0.25,0.45,0.8,1.5,2.75,5.0,8.0,15.0,25.0,45.0,80.0,140.0,250.0};
double pwr_threshold=-20.0;

double expected_timedelay(int delaycode) {
  int bit,i,code;
  double timedelay=0;
  code=delaycode;
  for (i=0;i<13;i++) {
    bit=(code & 0x1);
    timedelay+=bit*expected_timedelays[i];
    code=code >> 1;
  }
  return timedelay;
}

double phase_to_timedelay(double phase,double freq)
{
/*
* phase in degrees  freq in Hz timedelay in ns   ratio (0-1)
*/
  double timedelay=0;
  timedelay=phase/360.0/freq*1E9; // nanoseconds
  return -timedelay;
}
int main()
{
  int num_phasecodes[MAX_CARDS],num_cards[MAX_CARDS],num_freqs[MAX_CARDS],active[MAX_CARDS];
  int i,b,c,ii,cc,count,index,summary_freqs;
  int f,d,p,o;
  double offset,slope,slope_variance,sumsq;
  double best_sumsq[MAX_PHASES],best_offset[MAX_PHASES],best_slope[MAX_PHASES],best_var[MAX_PHASES];
  int best_o[MAX_PHASES];
  char tmp;
  int lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  double lowest_pwr_mag=1E10; // freq,card,phasecode
  int highest_time_delay_card[MAX_FREQS]; // freq
  double highest_time_delay[MAX_FREQS];
  double *freq[MAX_CARDS],*phase[MAX_FREQS][MAX_CARDS],*pwr_mag[MAX_FREQS][MAX_CARDS],*timedelay[MAX_FREQS][MAX_CARDS];    
  double last_timedelay=0;
  double Y[MAX_FREQS],T[MAX_FREQS],T_p0[MAX_FREQS],time0[MAX_FREQS];    
  double mindiff;
  double minT,maxY;
  double T_diff,T_360,expected,sum;
  int o_0;
  double offset_0,phase_0;
  char filename[120];
  int errflag=0;
  for(i=0;i<MAX_FREQS;i++) {
    highest_time_delay_card[i]=-1;
    highest_time_delay[i]=-1000;
  }
  if (verbose > 1 ) fprintf(stdout,"Nulling arrays\n");
  for(c=MIN_CARD;c<=MAX_CARD;c++) {
    active[c]=0;
    freq[c]=NULL;
    num_freqs[c]=0;
    for(i=0;i<MAX_FREQS;i++) {
        phase[i][c]=NULL;
        pwr_mag[i][c]=NULL;
        timedelay[i][c]=NULL;
    }
  }
  for(c=MIN_CARD;c<=MAX_CARD;c++) {
    sprintf(filename,"%s/phasing_cal_%s_%d.dat",dirstub,radar_name,c);
    calfile=fopen(filename,"r");
    if (verbose > 1 ) fprintf(stdout,"Opening: %p %s\n",calfile,filename); 
    if (calfile!=NULL) {
      errflag=0;
      active[c]=1;
      fread(&num_phasecodes[c],sizeof(int),1,calfile);
      if (verbose > 1 ) fprintf(stdout,"PhaseCodes: %d\n",num_phasecodes[c]);
      fread(&num_cards[c],sizeof(int),1,calfile);
      if (verbose > 1 ) fprintf(stdout,"Cards: %d\n",num_cards[c]);
      fread(&num_freqs[c],sizeof(int),1,calfile);
      if (verbose > 1 ) fprintf(stdout,"Freqs: %d\n",num_freqs[c]);
      if (num_freqs[c]>MAX_FREQS) {
        fprintf(stderr,"Too many stored frequencies...up the MAX_FREQS define!\n");
        exit(0);
      }

      if (verbose > 1 ) fprintf(stdout,"Allocating arrays\n");
      for(i=0;i<num_freqs[c];i++) {
        if (freq[c]!=NULL) free(freq[c]);
        freq[c]=calloc(num_freqs[c],sizeof(double));
        if (phase[i][c]!=NULL) free(phase[i][c]);
        phase[i][c]=calloc(num_phasecodes[c],sizeof(double));
        if (pwr_mag[i][c]!=NULL) free(pwr_mag[i][c]);
        pwr_mag[i][c]=calloc(num_phasecodes[c],sizeof(double));
        if (timedelay[i][c]!=NULL) free(timedelay[i][c]);
        timedelay[i][c]=calloc(num_phasecodes[c],sizeof(double));
      }


      if (verbose > 1 ) fprintf(stdout,"Reading frequency array\n");

      count=fread(freq[c],sizeof(double),num_freqs[c],calfile);
      if (verbose > 1 )fprintf(stdout,"%d %d\n",num_freqs[c],count);
      count=1;
      if (verbose > 1 ) fprintf(stdout,"Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int),1,calfile);
          if (count==0) {
            break;
          }
          count=fread(phase[ii][c],sizeof(double),num_phasecodes[c],calfile);
          if (verbose > 1) fprintf(stdout,"Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(pwr_mag[ii][c],sizeof(double),num_phasecodes[c],calfile);
          if (verbose > 1) fprintf(stdout,"Freq index: %d Pwr-mag Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
      }
      if (count==0) {
        if (feof(calfile)) if (verbose > 1 ) fprintf(stdout,"End of File!\n");
      }
      fclose(calfile);
      if (verbose > 0 ) fprintf(stdout,"Processing Phase Information for Card: %d\n",c);
      o_0=20;
      for (p=0;p<num_phasecodes[c];p++) {
        expected=0.0;
        for( b=0;b<13;b++) {
          index=(int)pow(2,b);
          if ( (p & index ) == index ) expected+=expected_timedelays[b];
        }
        best_offset[p]=1000;
        best_sumsq[p]=1E100;
        mindiff=1E8;
        for (o=o_0;o>-20;o--) {
          maxY=-100*360;
          offset=360.0*o;
          for (i=0;i<num_freqs[c];i++) {
            Y[i]=phase[i][c][p]+offset;
            T[i]=phase_to_timedelay(Y[i],freq[c][i]);
            if (p==0) T_p0[i]=0.0;
            T_360=phase_to_timedelay(-360.0,freq[c][i]);
            T_diff=fabs(T[i]-T_p0[i]-expected);
            if (T_diff < mindiff) {
              mindiff=T_diff;
              best_o[p]=o;
              best_offset[p]=offset; 
              best_sumsq[p]=sumsq;
              best_var[p]=slope_variance;
              best_slope[p]=slope;
            }
            if (p==0 ) {
              if(Y[i]>maxY) {
                maxY=Y[i];
                minT=phase_to_timedelay(maxY,freq[c][i]);
              }  
            } else {
              if (T_diff < mindiff) {
                mindiff=T_diff;
                best_o[p]=o;
                best_offset[p]=offset;
                best_sumsq[p]=sumsq;
                best_var[p]=slope_variance;
                best_slope[p]=slope;
              }
            }
//            if ((p==2048) && (i==0)) printf("%d :: o:%2d offset:%8.3lf t360: %8.3lf expected:%8.3lf measured:%8.3lf T_p0: %8.3lf diff: %8.3lf\n",i,o,offset,T_360,expected,T[i],T_p0[i],T_diff);

          }
/*
*  Linear Regression Y=slope*freq
*/

          if (p==0 ) {
            gsl_fit_mul (freq[c], 1, Y, 1, num_freqs[c], &slope, &slope_variance, &sumsq);
            if ((minT > 0) ) {
              if (sumsq < best_sumsq[p]) {
                best_o[p]=o;
                best_offset[p]=offset; 
                best_sumsq[p]=sumsq;
                best_var[p]=slope_variance;
                best_slope[p]=slope;
              }
            }
          }
        } // end offset loop
        if (verbose > 1 ) fprintf(stdout,"%d :: Best Offset: %lf %d\n",p,best_offset[p],best_o[p]);
        if (p==0) {
          phase_0=phase[0][c][p];
          offset_0=best_offset[p];
          o_0=best_o[p];
          for (i=0;i<num_freqs[c];i++) {
           T_p0[i]=phase_to_timedelay(phase_0+offset_0,freq[c][i]);
          }
          if (verbose > 1 ) fprintf(stdout,"%d :: Best Offset: %lf %d\n",p,best_offset[p],best_o[p]);
        }
/*
*  Build the timedelays arrays using the best phase offset from the linear regression
*/
        for (i=0;i<num_freqs[c];i++) {
          time0[i]=phase_to_timedelay(phase[i][c][0]+best_offset[0],freq[c][i]);
          timedelay[i][c][p]=phase_to_timedelay(phase[i][c][p]+best_offset[p],freq[c][i]);
          if(pwr_mag[i][c][p] < pwr_threshold) {
            if(errflag==0) fprintf(stderr,"ERROR: pwr_mag switch error: %lf Freq: %lf Code: %d Card: %d\n",pwr_mag[i][c][p],freq[c][i],p,c);
            errflag=1;
          }
        }  //freq loop
      }  //phasecode loop
      for(i=0;i<num_freqs[c];i++) {
        if (timedelay[i][c][0]>highest_time_delay[i]) {
            highest_time_delay_card[i]=c;
            highest_time_delay[i]=timedelay[i][c][0];
        }        
        for (b=0;b<num_phasecodes[c];b++) {
          if (pwr_mag[i][c][b]<lowest_pwr_mag) {
            lowest_pwr_mag_index[0]=i;
            lowest_pwr_mag_index[1]=c;
            lowest_pwr_mag_index[2]=b;
            lowest_pwr_mag=pwr_mag[i][c][b];
          }        
        }
      }
    }
    fprintf(stdout,"Calibration Stats:: %s :: Card: %d :: Freq: %ld\n",radar_name,c,freq[c][0]);
    fprintf(stdout,"Common Time Delay Offset(ns): %lf\n",time0[0]);
    fprintf(stdout,"Code:    0 :: Expected(ns): %8.3lf Measured(ns): %8.3lf Phase Offset: %8.3lf Final Phase: %8.3lf\n", 0.0,timedelay[0][c][0]-time0[0],best_offset[0],phase[0][c][0]+best_offset[0]);
    sum=0.0;
    for(b=0;b<13;b++) {
      index=(int)pow(2,b);
      sum+=expected_timedelays[b];
      fprintf(stdout,"Code: %4d :: Expected(ns): %8.3lf Measured(ns): %8.3lf Phase Offset: %8.3lf Final Phase: %8.3lf\n",
        index, expected_timedelays[b],timedelay[0][c][index]-time0[0],best_offset[index],phase[0][c][index]+best_offset[index]);
    }
    fprintf(stdout,"Code: 8191 :: Expected(ns): %8.3lf Measured(ns): %8.3lf Phase Offset: %8.3lf Final Phase: %8.3lf\n", sum,timedelay[0][c][8191]-time0[0],best_offset[8191],phase[0][c][8191]+best_offset[8191]);
    sprintf(filename,"%s/timedelay_cal_%s_%d.dat",dirstub,radar_name,c);
    timedelayfile=fopen(filename,"w+");
    if (verbose > 1 ) fprintf(stdout,"Creating: %p %s\n",timedelayfile,filename); 
    count=num_phasecodes[c];
    fwrite(&count,sizeof(int),1,timedelayfile);
    count=MAX_CARDS;
    fwrite(&count,sizeof(int),1,timedelayfile);
    count=num_freqs[c];
    fwrite(&count,sizeof(int),1,timedelayfile);
    count=0;
    fwrite(freq[c],sizeof(double),num_freqs[c],timedelayfile);
    for(i=0;i<num_freqs[c];i++) {
      if (verbose > 1) fprintf(stdout,"Freq %lf:  Time_0:%lf Time_8191: %lf\n",freq[c][i],timedelay[i][c][0],timedelay[i][c][8191]);
      fwrite(&i,sizeof(int),1,timedelayfile);
      count=fwrite(timedelay[i][c],sizeof(double),num_phasecodes[c],timedelayfile);
      count=fwrite(pwr_mag[i][c],sizeof(double),num_phasecodes[c],timedelayfile);
    }
    if (verbose > 1 ) fprintf(stdout,"Closing timedelay File\n");
    fclose(timedelayfile);
    if(freq[c]!=NULL) free(freq[c]);
    freq[c]=NULL;
    for(i=0;i<MAX_FREQS;i++) {
        if(phase[i][c]!=NULL) free(phase[i][c]);
        if(timedelay[i][c]!=NULL) free(timedelay[i][c]);
        if(pwr_mag[i][c]!=NULL) free(pwr_mag[i][c]);
        phase[i][c]=NULL;  
        pwr_mag[i][c]=NULL;
    }
    fflush(stdout);
    fflush(stderr);
  }  // End card Loop
//Summary stats
  fprintf(stdout,"::: Summary Stats for All Cards :::\n");
  if(verbose > 1 ) {
    for(i=0;i<MAX_FREQS;i++) {
      if (highest_time_delay_card[i] >=0) {
        fprintf(stdout,"Highest 0-Time Delay: %lf\n",highest_time_delay[i]);
        fprintf(stdout,"  freq: %d card: %d\n",
          i,highest_time_delay_card[i]);
      }
    } //End of Freq Loop
  }
  fprintf(stdout,"Lowest Mag: %lf\n",lowest_pwr_mag);
  fprintf(stdout,"Lowest Mag Index:: freq: %d card: %d phasecode: %d\n",
     lowest_pwr_mag_index[0],lowest_pwr_mag_index[1],lowest_pwr_mag_index[2]);
} // end of main

