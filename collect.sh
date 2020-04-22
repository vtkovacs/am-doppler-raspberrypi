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

