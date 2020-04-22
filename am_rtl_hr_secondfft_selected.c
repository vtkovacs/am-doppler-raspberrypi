/*
 * post process the output from am_rtl_channels
 * requires four arguments:
 * 1) the channels_subchannels file
 * 2) a file name prefix (same as used for am_rtl_channels)
 * 3) a directory name for the output files
 * 4) the number of data sets to process - if 0 process to eof
 *
 * For each line in the channels_subchannels file open the corresponding output from am_rtl_channels
 * and fft NSAMPLES (8192) data sets to eof. For each fft save the subchannel range
 * of power values to a new file.
 * There will be one output file for each line in the channels_subchannels file
 *
 * A "high resolution" procedure is used: the first fft will use the first 8192 samples then
 * subsequent fft's will shift the high 4096 samples of the previous input data down to the low 4096
 * and read 4096 more from the input to fill the high samples
 * Questions - tkovacs@dartmouth.edu
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "mailbox.h"
#include "gpu_fft.h"

#define PI 3.14159265359
#define NSAMPLES 8192
#define NCHANNELS 300

typedef struct GPU_FFT_COMPLEX COMPLEX;

unsigned Microseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec*1000000 + ts.tv_nsec/1000;
}

int main (int argc, char *argv[])
{
	int mb=mbox_open();
	int i, m, n, nn, f, nreads = 0, q;
	char *fname;
	char *fname_prefix;
	char *output_dir;
	int nchannels;
	int nupper;
	float w[NSAMPLES];
	float power[NSAMPLES];
	unsigned short ipower[NSAMPLES];
	struct datum {
		float re;
		float im;
	};
	struct datum data[NSAMPLES];

	FILE *ifile;
	FILE *ofile;
	FILE *channelfile;
	struct CHANNEL {
		char ifname[100];
		char ofname[100];
		int index;
		int subchan_start;
		int subchan_stop;
		int num_subchan;
	};

	struct CHANNEL channel[NCHANNELS];

	struct GPU_FFT_COMPLEX *input, *output;
	struct GPU_FFT *fft;
	time_t current_time;
	struct tm *timeinfo; 
	int printstat = 1;
	int nbytes = 0;
	if (argc < 5){
		printf("usage: %s channel_file file_prefix output_dir numfft\n", argv[0]);
		exit(-1);
	}
	fname_prefix = argv[2];
	output_dir = argv[3];
	int numreads = atoi(argv[4]);
	if ( numreads  <= 0){
		numreads = -numreads;
		printstat = 0;
	}
	channelfile=fopen (argv[1],"r");  // channels to process, subchannels to save - index into fft
	for (i = 0; i < NCHANNELS; i++){
		if (fscanf (channelfile, "%d %d %d", &channel[i].index, &channel[i].subchan_start, &channel[i].subchan_stop) != 3) break;
		sprintf(channel[i].ifname, "%s.%04d", fname_prefix, channel[i].index);
		sprintf(channel[i].ofname, "%s/%s.%04d.%04d-%04d", output_dir, fname_prefix, channel[i].index, channel[i].subchan_start, channel[i].subchan_stop);
		channel[i].subchan_start--;  // we count from 0
		channel[i].subchan_stop--;  // we count from 0
		channel[i].num_subchan = channel[i].subchan_stop - channel[i].subchan_start + 1;
	}
	fclose(channelfile);
	nchannels = i;
	/* Hanning window */
	for(i=0; i<NSAMPLES; i++){
		w[i] = 0.5 * (1 - cos(2*PI*(i) / (NSAMPLES-1)));
	}
	for (i = 0; i < nchannels; i++){
		ifile = fopen(channel[i].ifname, "r");
		ofile = fopen(channel[i].ofname, "w");
		gpu_fft_prepare(mb,13,GPU_FFT_FWD,1,&fft);
		input = fft->in;
		output = fft->out;
		nbytes = sizeof(data);
		nreads = 0;
		/*
		 * Prime the pump with the first NSAMPLES of data
		 */
		if (fread(data, nbytes, 1, ifile) != 1){
			if (printstat) fprintf(stderr, "%s end of data at %d\n", argv[0], nreads);
			exit(-1);
		}
		for (n=0; n<NSAMPLES; n++){
			input[n].re = data[n].re * w[n];
			input[n].im = data[n].im * w[n];
		}
		nreads++;
		while (numreads == 0 || nreads <= numreads){

			gpu_fft_execute(fft);
			/*
			 * calculate power and swap the data
			 */
			for (n=0; n<NSAMPLES/2; n++){
				nupper = n + NSAMPLES/2;
				power[n] = output[nupper].re * output[nupper].re + output[nupper].im * output[nupper].im;
				power[nupper] = output[n].re * output[n].re + output[n].im * output[n].im;
				power[n] = 10.0 * log10f(power[n]);
				ipower[n] = 10.0 * power[n];
				power[nupper] = 10.0 * log10f(power[nupper]);
				ipower[nupper] = 10.0 * power[nupper];
			}

			fwrite(&ipower[channel[i].subchan_start], channel[i].num_subchan*2, 1, ofile);
			/*
			 * move the upper data to the lower half of the input for the next fft
			 */
			for (n=0, nn = NSAMPLES/2; n < NSAMPLES/2; n++, nn++){
				input[n].re = data[nn].re;
				input[n].im = data[nn].im;
			}
			/*
			 * get the next NSAMPLES/2 of data into the upper half
			 */
			if (fread(&data[NSAMPLES/2], nbytes/2, 1, ifile) != 1){
				if (printstat) fprintf(stderr, "%s end of data at %d\n", argv[0], nreads);
				break;
			}
			for (n = NSAMPLES/2; n < NSAMPLES; n++){
				input[n].re = data[n].re;
				input[n].im = data[n].im;
			}
			for (n = 0; n < NSAMPLES; n++){
				input[n].re *= w[n];
				input[n].im *= w[n];
			}
			nreads++;
		}
		if (printstat) printf("%d\n", nreads);
		gpu_fft_release(fft);
		fclose(ifile);
		fclose(ofile);
	}

}
