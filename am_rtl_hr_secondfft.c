/*
 * the first fft will use the first 8192 samples
 * subsequent fft's will shift the high 4096 samples of the previous input data down to the low 4096
 * and read 4096 more from the input to fill the high samples
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
	unsigned t[10];
	float w[NSAMPLES];
	float power[NSAMPLES];
	struct datum {
		float re;
		float im;
	};
	struct datum data[NSAMPLES];

	FILE *ifile;
	FILE *ofile;
	struct GPU_FFT_COMPLEX *input, *output;
	struct GPU_FFT *fft;
	time_t current_time;
	struct tm *timeinfo; 
	int printstat = 1;
	int nbytes = 0;  /* set constants */
	if (argc < 4){
		printf("usage: %s infile ofile numfft\n", argv[0]);
		exit(-1);
	}
	int numreads = atoi(argv[3]);
	if ( numreads  <= 0){
		numreads = -numreads;
		printstat = 0;
	}

	fname = argv[1];
	if ( ! strncmp(fname, "-", 1)){
		ifile = stdin;
	} else {
		ifile=fopen(fname, "r");
	}

	fname = argv[2];
	if ( ! strncmp(fname, "-", 1)){
		ofile = stdout;
	printstat = 0;
	} else {
		ofile=fopen(fname, "w");
	}

	/* Hanning window */
	for(i=0; i<NSAMPLES; i++){
		w[i] = 0.5 * (1 - cos(2*PI*(i) / (NSAMPLES-1)));
	}
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
	while (numreads == 0 || nreads < numreads){

		gpu_fft_execute(fft);

		for (n=0; n<NSAMPLES; n++){
			power[n] = output[n].re * output[n].re + output[n].im * output[n].im;
			power[n] = 10.0 * log10f(power[n]);
		}

		/*
		 * swap the upper and lower halves of the fft using two writes
		 */
		fwrite(&power[NSAMPLES/2], (NSAMPLES/2)*4, 1, ofile);
		fwrite(power, (NSAMPLES/2)*4, 1, ofile);
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
	if (printstat) printf("\n");
	gpu_fft_release(fft);

}
