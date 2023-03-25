#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>

define N = 1024; //Matrix number N*N

//claim function
int ** makeMatrixA(int N);
int * makeMatrixx(int N);

//this is main function
int main(int argc, char *argv[]){
	// init the MPI
	int rank, size;
	MPI_Init(&argc, &argv);    //MPI Initialize
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);      //obtain the rank id
	MPI_Comm_size(MPI_COMM_WORLD, &size);      //obtain the number of processes
	
	//start time
	MPI_Barrier(comm);
	double local_start, local_end;
	local_start = MPI_Wtime;

	//make Matrix
	int ** MatrixA = makeMatrixA(N);
	int * Matrixx = makeMatrixx(N);

	//each process calculate the result
	int * y = (int *)malloc(sizeof(int)*N);
	int * localy = (int *)malloc(sizeof(int)*N/size);
	int localN = N/size;
	for(int i=0; i<localN; i++){
		for(int j=0; j<N; j++){
			localy[i] += MatrixA[rank*localN][j] * Matrixx[j];
		}	
	}
	
	//end time
	local_end = MPI_Wtime;

	//get result together from all process
	MPI_Gather(localy, localN, MPI_INT, y, localN, MPI_INT, 0, MPI_COMM_WORLD);
	
	//print result
	if(rank == 0){
		printf("Execute time is %d", local_end-local_start);
		printf("The result is:")
		for(int i=0; i<yN; i++){
			printf("%d ", y[i])
		}
	}
	
	MPI_Finalize(); //Finalize MPI
	
}
int ** makeMatrixA(int N){
	MatrixA=int(**)malloc(sizeof(int *)*N);
	for(int i=0; i<N; i++){
		MatrixA=(int *)malloc(sizeof(int)*N);
	}
	for(int i=0, i<N; i++){
		for(int j=0; j<N; j++){
			if(j==(i-1)||j==(i+1)){
				MatrixA=-1;
			}
			else if(j==i){
				MatrixA=2;
			}	
			else{
				MatrixA=0;
			}

		}
	}
	return MatrixA;
}
int * makeMatrixx(int N){
	Matrixx=(int *)malloc(sizeof(int)*N);
	for(int i=0; i<N; i++){
		if(i%3 == 0){
			Matrixx=1;
		}
		else if(i%3 ==1){
			Matrixx=2;
		}
		else{
			Matrixx=3;
		}
	}
	return Matrixx;
}
