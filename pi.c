#include <omp.h>
#include <stdio.h>
#define MAX_THREADS 8

static long steps = 1000000000;
double step;

int main (int argc, const char *argv[]) {
    int i,j;
    double x;
    double pi, sum = 0.0;
    double start, delta;

    step = 1.0/(double) steps;

    // Compute parallel compute times for 1-MAX_THREADS
    for (j=1; j<= MAX_THREADS; j++) {
        int num_th, num_th_max;

        // This is the beginning of a single PI computation
        omp_set_num_threads(j);
        // Set the number of threads for the parallel computing
/*
        #pragma omp parallel
        #pragma omp master
        {
          printf("The number of threads used %d; The max number of threads can be used %d; The hardware limits %d\n",
               omp_get_num_threads(),
               omp_get_max_threads(),
               omp_get_num_procs());
        }
*/
        printf("  Running on %d threads: ", j);
        sum = 0.0;
        double start = omp_get_wtime();
        // Get the wall time
        #pragma omp parallel for reduction(+:sum) private(x)
        for (i=0; i < steps; i++) {
            x = (i+0.5)*step;
            sum += 4.0 / (1.0+x*x);
        }
        // Out of the parallel region, finialize computation
        pi = step * sum;
        delta = omp_get_wtime() - start;
        printf("PI = %.16g computed in %.4g seconds\n", pi, delta);
    }
}
