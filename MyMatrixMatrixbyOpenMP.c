#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

//Matrix number N*N
#define N 16384

//claim function
int ** makeMatrixA(int matrixsize);
int * makeMatrixx(int matrixsize);

//this is main function
int main(int argc, char *argv[]){
	//make Matrix
	int ** MatrixA = makeMatrixA(N);
	int * Matrixx = makeMatrixx(N);

	//start time
	double start_time, end_time;
	start_time = omp_get_wtime();

	//malloc memory to store result
	int * y = (int *)malloc(sizeof(int)*N);

	#pragma omp parallel for default(shared) num_threads(4) 
	for(int i=0; i<N; i++){
		y[i]=0;
		for(int j=0; j<N; j++){
			y[i] += MatrixA[i][j] * Matrixx[j];
		}	
	}
	
	//end time
	end_time = omp_get_wtime();
	
	//print result
	printf("Execute time is %lf.\n", end_time-start_time);
	printf("The result is:");
	for(int i=0; i<N; i++){
		printf("%d ", y[i]);
	}
	printf("\n");
	
	// free memory
	free(MatrixA);
	free(Matrixx);
	free(y);
}

int ** makeMatrixA(int matrixsize){
	int **MatrixA = (int **)malloc(sizeof(int *)*matrixsize);
	for(int i=0; i<N; i++){
		MatrixA[i] = (int *)malloc(sizeof(int)*matrixsize);
	}
	for(int i=0; i<matrixsize; i++){
		for(int j=0; j<matrixsize; j++){
			if(j==(i-1)||j==(i+1)){
				MatrixA[i][j]=-1;
			}
			else if(j==i){
				MatrixA[i][j]=2;
			}	
			else{
				MatrixA[i][j]=0;
			}

		}
	}
	return MatrixA;
}

int * makeMatrixx(int matrixsize){
	int * Matrixx=(int *)malloc(sizeof(int)*matrixsize);
	for(int i=0; i<matrixsize; i++){
		if(i%3 == 0){
			Matrixx[i] = 1;
		}
		else if(i%3 ==1){
			Matrixx[i] = 2;
		}
		else{
			Matrixx[i] = 3;
		}
	}
	return Matrixx;
}
