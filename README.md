
For realtime and post processing of data collected with rtl_sdr.
This repo also includes a copy of [GPU_FFT by Andrew Holme](http://www.aholme.co.uk/GPU_FFT/Main.htm).
Prerequisites are gcc and rtl_sdr (apt install rtl-sdr)

## am_rtl_channels 
requires four arguments:
 *  an input file name ( use - for stdin )
 *  The number of NSAMPLES (8192) data sets to process (numreads)
 *  a file containing the channels to save. Only the first number (channel) of each line is used
 *  a prefix for the file names to be written
 
 There will be one output file for each line in the channels file and the fft_out[channel] is saved to that file.
 The process exits on end of data or after numreads.
 
## am_rtl_hr_second_selected 
post process the output from am_rtl_channels and also requires four arguments:
 *  the channels_subchannels file
 *  a file name prefix (same as used for am_rtl_channels)
 *  a directory name for the output files
 *  the number of data sets to process - if 0 process to eof
 
 For each line in channels_subchannels the corresponding output from am_rtl_channels is processed.
 Sets of NSAMPLES (8192) of data are processed to eof. For each FFT the subchannel range of power values are saved to a new file.
 One output file for each line in channels_subchannels is produced.

## Sample script collect.sh
```
#!/bin/bash
# the number of 8192 point FFTs to collect 
NFFT=$1
COLLECT_DIR=`date +%d.%m.%y_%H:%M:%S`
FNAME_PREFIX=${COLLECT_DIR}_1MSs
mkdir $COLLECT_DIR
#copy myself for documentation
cp $0 $COLLECT_DIR
#record hostname for documentation
hostname -A > $COLLECT_DIR/hostname
cd $COLLECT_DIR
/usr/bin/rtl_sdr -g 10 -s 1000000 -f 130.7e6 -b 16384 - 2> rtl_sdr.out | /usr/local/bin/am_rtl_channels - $NFFT ../channels_subchannels $FNAME_PREFIX > time_errors 2>&1
mkdir FFT
/usr/local/bin/am_rtl_hr_secondfft_selected ../channels_subchannels $FNAME_PREFIX FFT 0
```
## channels_subchannels
Triplets of integers:
```
           1        5230        5766
          83        4558        5094
         165        3859        4395
         247        2964        3500
         329        2651        3187
         411        2622        3158
 ```
