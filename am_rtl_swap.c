
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
#define NSAMPLES_2 8192
#define MAXLEN 50
#define NSTATION 9


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
	int idx[NSAMPLES];
	float df;
	char *fname;
	unsigned t[10];
	long nbytes, sr, start;
	long freqs[NSTATION];
	double w[NSAMPLES], w_1[NSAMPLES_2];
	struct datum {
		unsigned char re;
		unsigned char im;
	};
	struct datum data[NSAMPLES];
	struct GPU_FFT_COMPLEX average[NSAMPLES];

	FILE *ifile;
	FILE *ofile;
	FILE *kfile;
	struct GPU_FFT_COMPLEX *input, *output;
	struct GPU_FFT *fft;
	time_t current_time;
	char newfname[MAXLEN];
	struct tm *timeinfo; 
	int printstat = 1;
	nbytes=0;  /* set constants */
	sr=1e6;
	start=0.6e6;
	df=sr/NSAMPLES;
	nreads=floor (nbytes/2/NSAMPLES);
	if (argc < 4){
		printf("usage: %s infile ofile numfft\n", argv[0]);
		exit(-1);
	}
	int numreads = atoi(argv[3]);

	for (m=0; m<NSAMPLES; m++) /* index array */
		idx[m]=m;

//	kfile=fopen ("frequency_selection","r"); /* frequency selection */
//	for (q=0;q<NSTATION;q++)
//		fscanf (kfile, "%l", &freqs[q]);

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
//		printf("%d, %f\n", i, w[i]);
	}
	for(i=0; i<NSAMPLES_2; i++){
		w_1[i] = 0.5 * (1 - cos(2*PI*(i) / (NSAMPLES_2-1)));
	}
	gpu_fft_prepare(mb,13,GPU_FFT_FWD,1,&fft);
	input = fft->in;
	output = fft->out;
	/* read from raw data */
//	for (n=0;n<nreads;n++){
	nbytes = sizeof(data);
	nreads = 0;
	while (nreads++ < numreads){
        t[0] = Microseconds();
		if (fread(data, nbytes, 1, ifile) != 1){
			fprintf(stderr, "fft end of data\n");
			break;
		}
		t[1] = Microseconds();
		for (n=0; n<NSAMPLES; n++){
			input[n].re = data[n].re;
			input[n].re -= 127.0;
//			input[n].re *= w[n];
			input[n].im = data[n].im;
			input[n].im -= 127.0;
//			input[n].im *= w[n];
//			printf("%d, %d, %d, %f, %f\n", n, data[n].re, data[n].im, input[n].re, input[n].im);
		}
		t[2] = Microseconds();
		gpu_fft_execute(fft);
        t[3] = Microseconds();
		if (printstat) printf("%6d %6d %6d %6d %6d \n", nreads , t[1] -t[0], t[2] - t[1], t[3] - t[2], t[3] - t[0]);
//		for(n = 0; n < NSAMPLES; n++){
//			printf("%d, %f, %f\n", n, output[n].re, output[n].im);
//			fwrite(&output[n].re, 4, 1, ofile);
//			fwrite(&output[n].im, 4, 1, ofile);
//		}
//		fwrite(&output[NSAMPLES/2], (NSAMPLES/2)*8, 1, ofile);
		fwrite(&output[NSAMPLES/2], (NSAMPLES/2)*8, 1, ofile);
		fwrite(output, (NSAMPLES/2)*8, 1, ofile);

	}
	if (printstat) printf("\n");
	gpu_fft_release(fft);

}
