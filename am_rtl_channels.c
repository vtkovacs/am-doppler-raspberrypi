/*
 * Requires four arguments:
 * 1) an input file name or - for stdin
 * 2) The number of NSAMPLES (8192) data sets to process - numreads
 * 3) a file containing the channels to save. Only the first number (channel) of each line is used
 * 4) a prefix for the file names to be written
 *
 * There will be one output file for each line in the channels file and
 * the fft_out(channel) is saved to that file
 * process exits on end of data or after numreads
 *
 * Questions - tkovacs@dartmouth.edu
 */
#include <stdlib.h>
#include <unistd.h>
#include <linux/fcntl.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "mailbox.h"
#include "gpu_fft.h"

#define PI 3.14159265359
#define NSAMPLES 8192
#define NSAMPLES_2 8192
#define MAXLEN 50
#define NCHANNELS 300
#define REPCOUNT 1000
#define ELAPSE_ERROR_REPORT 10000  // report errors bigger than this microseconds

typedef struct GPU_FFT_COMPLEX COMPLEX;

unsigned long long Microseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec*1000000LL + ts.tv_nsec/1000LL;
}

int main (int argc, char *argv[])
{
	int mb=mbox_open();
	int i, m, n, n_1, f, nreads = 0, q;
	int nchannels;
	char *fname;
	unsigned long long t[10], tstart = 0, tStart;
	unsigned long long elapse_clock; // clock elapsed time
	unsigned long long elapse_data; // data derived elapsed time (assumes 1 MS/s)
	long long elapse_error, elapse_error_last;
	int reportcount = 0;
	long nbytes;
	double w[NSAMPLES], w_1[NSAMPLES_2];
	struct datum {
		unsigned char re;
		unsigned char im;
	};
	struct datum data[NSAMPLES];
	struct GPU_FFT_COMPLEX average[NSAMPLES];

	struct CHANNEL {
		char fname[100];
		FILE *ofile;
		int index;
	};

	struct CHANNEL channel[NCHANNELS];

	FILE *ifile;
	FILE *channelfile;
	char *fname_prefix;
	struct GPU_FFT_COMPLEX *input, *output;
	struct GPU_FFT *fft;
	time_t current_time;
	char newfname[MAXLEN];
	struct tm *timeinfo;
	nbytes=0;  /* set constants */
	nreads=floor (nbytes/2/NSAMPLES);
	int numreads = atoi(argv[2]);

	channelfile=fopen (argv[3],"r");  // channels to save - index into fft
	if (channelfile == NULL) {
		printf("%s: unable to open %s for reading\n", argv[0], argv[3]);
		exit(-1);
	}
	fname_prefix = argv[4];
	for (i = 0; i < NCHANNELS; i++){
		if (fscanf (channelfile, "%d %*d %*d", &channel[i].index) != 1) break;
		sprintf(channel[i].fname, "%s.%04d", fname_prefix, channel[i].index);
		channel[i].ofile = fopen(channel[i].fname, "w");
		channel[i].index--;  // we count from 0
		if (channel[i].index >= NSAMPLES/2){  // fft bins need to be swapped
			channel[i].index -= NSAMPLES/2;
		} else {
			channel[i].index += NSAMPLES/2;
		}
	}
	channel[i].index = 0;
	nchannels = i;
	fname = argv[1];
	if ( ! strncmp(fname, "-", 1)){
		ifile = stdin;
	} else {
		ifile=fopen(fname, "r");
	}

	/* Hanning window */
//	for(i=0; i<NSAMPLES; i++){
//		w[i] = 0.5 * (1 - cos(2*PI*(i) / (NSAMPLES-1)));
//	}
//	for(i=0; i<NSAMPLES_2; i++){
//		w_1[i] = 0.5 * (1 - cos(2*PI*(i) / (NSAMPLES_2-1)));
//	}
	gpu_fft_prepare(mb,13,GPU_FFT_FWD,1,&fft);
	input = fft->in;
	output = fft->out;
	nbytes = sizeof(data);
	nreads = 0;
	tStart = Microseconds();
	printf("NFFT, clock time, data time, error\n");
	while (nreads++ < numreads){
        t[0] = Microseconds();
		if (fread(data, nbytes, 1, ifile) != 1){
			printf("fft end of data\n");
			break;
		}
		t[1] = Microseconds();
 		if ( tstart == 0 ) tstart = t[1];
		if (++reportcount == REPCOUNT){
			elapse_clock = t[1] - tstart;
			elapse_data = 8192LL * (nreads - 1);
			elapse_error = elapse_clock - elapse_data;
			if (labs(elapse_error - elapse_error_last) > ELAPSE_ERROR_REPORT){
				printf("%d %f %f %f\n", nreads, elapse_clock/1.0e6, elapse_data/1.0e6, elapse_error/1.0e6);
				elapse_error_last = elapse_error;
				fflush(stdout);
			}

			reportcount = 0;
		}
		for (n=0; n<NSAMPLES; n++){
			input[n].re = data[n].re;
			input[n].re -= 127.0;
//			input[n].re *= w[n];
			input[n].im = data[n].im;
			input[n].im -= 127.0;
//			input[n].im *= w[n];
		}
		t[2] = Microseconds();
		gpu_fft_execute(fft);
        t[3] = Microseconds();
//		printf("%6d %6d %6d %6d %6d \n", nreads , t[1] -t[0], t[2] - t[1], t[3] - t[2], t[3] - t[0]);
		for(i = 0; i < nchannels; i++){
			fwrite(&output[channel[i].index], 8, 1, channel[i].ofile);
		}
		t[4] = Microseconds();
//		printf("%6d %6d %6d %6d %6d %6d\n", nreads , t[1] -t[0], t[2] - t[1], t[3] - t[2], t[4] - t[3], t[4] - t[0]);

	}
	elapse_data = 8192LL * (nreads - 1);  // time it should have taken at 1e6 samples/sec
	elapse_clock = Microseconds() - tstart; // clock time
	printf("\nData derived seconds: %f, clock seconds: %f\n", elapse_data / 1.0e6, elapse_clock / 1.0e6);

	gpu_fft_release(fft);

}
