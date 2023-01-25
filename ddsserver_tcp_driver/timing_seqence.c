/* Program ics660_timingseq */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <fcntl.h>
#ifdef __QNX__
  #include  <hw/pci.h>
  #include <sys/resource.h>
  #include <devctl.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <hw/inout.h>
#endif
#include "ics660b.h"
#include "dds_defs.h"

extern int verbose;
int timing_seqence(int seqlength ,int seq[],FILE *ics660[])
{
  int i,i1,j,ioff,pci_ind=0,pci_max,pci_min;
 long SC;
  int status;
  int bank_width;
  int bank_length;
  int transmit_length;
  int reset_value;
  int channel,chip,b_ind,seqlen;
  int enable = ENABLE;
  int external=(int)EXTERNAL;
  int pulse_len, nchan=16, ind, sig_lev;
  uint32_t tlen,blen;
  double freq,phi_inc,phi_dif;
  double phase,pi;
  float synth_state  = STATE_TIME;
  uint32_t *data_ar;
  union dac_samples {
    uint32_t two_samp;
    struct {
      short low;
      short high;
    } samples;
  } dac_value;
//  float max_val =0x2fff;
  float max_val=0xffff;
  float baudfact,codelen;
  char trans_seq[200000];
  float t_seq_state;
 int t_ratio,flag;
  if (verbose > 0) printf("In Old timing seqence\n");

  pi = (float)acosf(-1.0);
  pci_max = 0;
  pci_min = 0;
  //transmit_length=seqlength;
  transmit_length=0;
  t_seq_state=10e-6;/*10 micro second*/
  t_ratio=(int)(t_seq_state/synth_state+0.4999999);
  printf("time ratio = %d\n",t_ratio);
  /*unpacking the seqence*/
  for(i=0; i<seqlength;i+=2)
    for(j=0;j<seq[i+1];j++){
      trans_seq[transmit_length] =seq[i];
      transmit_length++;
     }
  if (verbose > 0) printf("seq length: %d state step: %lf time: %lf\n",
                                                transmit_length,STATE_TIME,STATE_TIME*transmit_length);

  printf("transmitlength = %d\n",transmit_length);
  SC=nchan*transmit_length*t_ratio;
// printf("ICS660_XMT SC = %d\n",SC);
#ifdef __QNX__
  for( pci_ind=pci_max; pci_ind>=pci_min; pci_ind--){
  
//     fflush(stdout);
   
    status=ics660_init(ics660[pci_ind],pci_ind);
  }



  data_ar = (uint32_t *)calloc((size_t)SC, (size_t)sizeof(uint32_t));
  printf("ICS660_XMT SC = %d\n",SC);
      flag=0;  
      for( i=0; i<transmit_length; i++){
        if (trans_seq[i]!=0 && flag==0) {
          printf("Beginning of TX: %d\n",i);
          flag=1;
        }
        if (trans_seq[i]==0 && flag==1) {
          printf("End of TX: %d\n",i-1);
          flag=0;
        }
        dac_value.samples.low = (short)trans_seq[i]*max_val;
        dac_value.samples.high =(short)trans_seq[i]*max_val;

        ioff = i*nchan*t_ratio;

        for( j=0; j<nchan*t_ratio; j++){

          ind = (ioff + j);
          data_ar[ind] = (uint32_t) dac_value.two_samp;
        }
      }
    
    
  printf("timing seq is ready to be sent\n");

  for( pci_ind=pci_max; pci_ind >= pci_min; pci_ind --){

      bank_length = SC/2-1;
      bank_width = 0;
      transmit_length = SC/2- 1;
      ics660_set_parameter((int)ics660[pci_ind],ICS660_BANK_LENGTH,&bank_length, sizeof(bank_length));
      ics660_set_parameter((int)ics660[pci_ind],ICS660_BANK_WIDTH,&bank_width, sizeof(bank_width));
      ics660_set_parameter((int)ics660[pci_ind],ICS660_TRANS_LENGTH,&transmit_length, sizeof(transmit_length));

      /* Write DAC Reset register */
      reset_value = RESET_VALUE;
      ics660_set_parameter((int)ics660[pci_ind],ICS660_DAC_RESET,&reset_value, sizeof(RESET_VALUE));

      write((int)ics660[pci_ind],data_ar,(size_t)4*(SC));
      write((int)ics660[pci_ind],data_ar,(size_t)4*(SC));
      
    }  
     /* Set DAC enable bit in control register */
  for( pci_ind=pci_max; pci_ind>=pci_min; pci_ind--)
    ics660_set_parameter((int)ics660[pci_ind],ICS660_DAC_ENABLE,&enable, sizeof(enable));

  for(pci_ind=pci_max; pci_ind>=pci_min; pci_ind--)
    ics660_set_parameter((int)ics660[pci_ind],ICS660_SET_DC_READY, &enable, sizeof(enable));
  
  for(pci_ind=pci_max; pci_ind>=pci_min; pci_ind--)
    ics660_set_parameter((int)ics660[pci_ind],ICS660_RELEASE_RESETS, &enable, sizeof(enable));
    
    
  free(data_ar);  
#endif
  return 0;

}
