
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
	int i, m, n, n_1, f, nreads = 0, q;
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
	while (numreads == 0 || nreads < numreads){
        t[0] = Microseconds();
		if (fread(data, nbytes, 1, ifile) != 1){
			if (printstat) fprintf(stderr, "%s end of data at %d\n", argv[0], nreads);
			break;
		}
		nreads++;
		t[1] = Microseconds();
		for (n=0; n<NSAMPLES; n++){
			input[n].re = data[n].re * w[n];
			input[n].im = data[n].im * w[n];
		}
		t[2] = Microseconds();
		gpu_fft_execute(fft);
        t[3] = Microseconds();
		for (n=0; n<NSAMPLES; n++){
			power[n] = output[n].re * output[n].re + output[n].im * output[n].im;
			power[n] = 10.0 * log10f(power[n]);
		}
		if (printstat) printf("%6d %6d %6d %6d %6d \n", nreads , t[1] -t[0], t[2] - t[1], t[3] - t[2], t[3] - t[0]);
//		for(n = 0; n < NSAMPLES; n++){
//			printf("%d, %f, %f\n", n, output[n].re, output[n].im);
//			fwrite(&output[n].re, 4, 1, ofile);
//			fwrite(&output[n].im, 4, 1, ofile);
//		}
		fwrite(&power[NSAMPLES/2], (NSAMPLES/2)*4, 1, ofile);
		fwrite(power, (NSAMPLES/2)*4, 1, ofile);
	}
	if (printstat) printf("\n");
	gpu_fft_release(fft);

}
