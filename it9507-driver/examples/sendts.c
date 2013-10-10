/* 

sendts.c - write a TS to the UT100-C DVB-T modulator

(C) Dave Chapman <dave@dchapman.com> 2013

Copyright notice:

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
    
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <linux/types.h>

#include "../include/dvbmod.h"

/* Function based on code in the tsrfsend.c application by Avalpa
   Digital Engineering srl */
static int calc_channel_capacity(TxAcquireChannelRequest *channel_request,
                                 SetModuleRequest *module_request)
{
  uint64_t channel_capacity;
  channel_capacity = channel_request->bandwidth * 1000;
  channel_capacity = channel_capacity * ( module_request->constellation * 2 + 2);

  switch (module_request->interval) {
  case Interval_1_OVER_32:
    channel_capacity = channel_capacity*32/33;
    break;
  case Interval_1_OVER_16:
    channel_capacity = channel_capacity*16/17;
    break;
  case Interval_1_OVER_8:
    channel_capacity = channel_capacity*8/9;
    break;
  case Interval_1_OVER_4:
    channel_capacity = channel_capacity*4/5;
    break;
  }
  switch (module_request->highCodeRate) {
  case CodeRate_1_OVER_2:
    channel_capacity = channel_capacity*1/2;
    break;
  case CodeRate_2_OVER_3:
    channel_capacity = channel_capacity*2/3;
    break;
  case CodeRate_3_OVER_4:
    channel_capacity = channel_capacity*3/4;
    break;
  case CodeRate_5_OVER_6:
    channel_capacity = channel_capacity*5/6;
    break;
  case CodeRate_7_OVER_8:
    channel_capacity = channel_capacity*7/8;
    break;
  }

  return channel_capacity/544*423;
}


int main(int argc, char* argv[])
{
  int mod_fd;
  int result;
  int channel_capacity;

  /* Open Device - hard-coded to device #1 */
  if ((mod_fd = open("/dev/dvbmod0", O_RDWR)) < 0) {
    fprintf(stderr,"Failed to open device.\n");
    return 1;
  }

  /* Set channel (frequency/bandwidth) */
  TxAcquireChannelRequest channel_request;
  channel_request.chip = 0;
  channel_request.frequency = 794000;
  channel_request.bandwidth = 8000;
  result = ioctl(mod_fd, IOCTL_ITE_DEMOD_ACQUIRECHANNEL_TX, &channel_request);

  /* Set modulation parameters */
  SetModuleRequest module_request;
  module_request.chip = 0;
  module_request.transmissionMode = TransmissionMode_8K;
  module_request.constellation = Constellation_64QAM;
  module_request.interval = Interval_1_OVER_4;
  module_request.highCodeRate = CodeRate_2_OVER_3;
  result = ioctl(mod_fd, IOCTL_ITE_DEMOD_SETMODULE_TX, &module_request);

  GetGainRangeRequest get_gain_range_request;
  int mingain,maxgain;
  get_gain_range_request.frequency = 794000;
  get_gain_range_request.bandwidth = 8000;
  get_gain_range_request.minGain = &mingain;
  get_gain_range_request.maxGain = &maxgain;
  result = ioctl(mod_fd, IOCTL_ITE_DEMOD_GETGAINRANGE_TX, &get_gain_range_request);
  fprintf(stderr,"Gain range: %d to %d\n",mingain,maxgain);

  SetGainRequest set_gain_request;
  set_gain_request.GainValue = -10;
  result = ioctl(mod_fd, IOCTL_ITE_DEMOD_ADJUSTOUTPUTGAIN_TX, &set_gain_request);  
  fprintf(stderr,"Gain set to %d\n",set_gain_request.GainValue);

  SetTPSRequest tps_request;
  memset(&tps_request, 0, sizeof(tps_request));
  tps_request.tps.cellid = 0;
  result = ioctl(mod_fd, IOCTL_ITE_DEMOD_SETTPS_TX, (void *)&tps_request);

  /* Calculate and display the channel capacity based on the modulation/channel parameters */
  channel_capacity = calc_channel_capacity(&channel_request,&module_request);
  fprintf(stderr,"Channel capacity = %dbps\n",channel_capacity);

  /* Start the transfer */
  result = ioctl(mod_fd, IOCTL_ITE_DEMOD_STARTTRANSFER_TX);

  /* The main transfer loop */
  unsigned char buf[188*1000];
  int n;
  unsigned long long bytes_sent = 0;
  while(1) {
    n = read(0,buf,sizeof(buf));
    if (n == 0) { break; }

    int to_write = n;
    int bytes_done = 0;
    while (bytes_done < to_write) {
      n = write(mod_fd,buf+bytes_done,to_write-bytes_done);
      if (n == 0) {
        /* This shouldn't happen */
        fprintf(stderr,"Zero write\n");
        usleep(500);
      } else if (n <= 0) {
	fprintf(stderr,"Write error %d: ",n);
        perror("Write error: ");
      } else {
        //if (n < sizeof(buf)) { fprintf(stderr,"Short write - %d bytes\n",n); }
        //fprintf(stderr,"Wrote %d\n",n);
        bytes_sent += n;
        bytes_done += n;
        //fprintf(stderr,"Bytes sent: %llu\r",bytes_sent);
      }
    }
  }  

  /* Stop the transfer */
  result = ioctl(mod_fd, IOCTL_ITE_DEMOD_STOPTRANSFER_TX);

  close(mod_fd);
  return 0;
}
