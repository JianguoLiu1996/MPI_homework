#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>

//Matrix number N*N
#define N 1024 

//claim function
int ** makeMatrixA(int matrixsize);
int * makeMatrixx(int matrixsize);

//this is main function
int main(int argc, char *argv[]){
	// init the MPI
	int rank, size;
	MPI_Init(&argc, &argv);    //MPI Initialize
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);      //obtain the rank id
	MPI_Comm_size(MPI_COMM_WORLD, &size);      //obtain the number of processes
	
	if(rank == 0){
		printf("The size is %d \n", size);
	}
	//start time
	double local_start, local_end;
	local_start = MPI_Wtime();

	//make Matrix
	int ** MatrixA = makeMatrixA(N);
	if(rank ==0){
		printf("MatrixA is: \n");
		for(int i=0; i<N; i++){
			for(int j=0; j<N; j++){
				printf("%d, ",MatrixA[i][j]);
			}
			printf("\n");
		}
	}
	int * Matrixx = makeMatrixx(N);
	if(rank ==0 ){
		printf("Matrixx is:\n ");
		for(int i=0; i<N; i++){
			printf("%d ", Matrixx[i]);
		}
		printf("\n");
	}

	//each process calculate the result
	int * y = (int *)malloc(sizeof(int)*N);
	int * localy = (int *)malloc(sizeof(int)*N/size);
	int localN = N/size;
	for(int i=0; i<localN; i++){
		localy[i]=0;
		for(int j=0; j<N; j++){
			localy[i] += MatrixA[rank*localN+i][j] * Matrixx[j];
			if(rank == 0){
				printf("%d ", MatrixA[rank*localN+i][j]*Matrixx[j]);
			}
		}	
		printf("\n");
	}
	if(rank ==0){
		printf("\n process 0 local y is:\n");
		for(int i=0; i<localN; i++){
			printf("%d ", localy[i]);
		}
		printf("\n");
	}
	
	//MPI_Barrier(MPI_COMM_WORLD);
	//end time
	local_end = MPI_Wtime();

	//get result together from all process
	MPI_Gather(localy, localN, MPI_INT, y, localN, MPI_INT, 0, MPI_COMM_WORLD);
	
	//print result
	if(rank == 0){
		printf("Execute time is %lf.\n", local_end-local_start);
		printf("The result is:");
		for(int i=0; i<N; i++){
			printf("%d ", y[i]);
		}
		printf("\n");
	}
	
	// Print a message from each process
	MPI_Finalize(); //Finalize MPI
	
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
