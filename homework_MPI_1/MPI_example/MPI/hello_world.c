/*
** A very simple example MPI program, written in C
*/

#include <stdio.h>
/* "mpi.h" contains function prototypes and macros we'll need */ 
#include "mpi.h"


int main(int argc, char* argv[])
{
  int rank;               /* 'rank' of process among it's cohort */ 
  int size;               /* size of cohort, i.e. num processes started */

  /* initialise our MPI environment */
  MPI_Init( &argc, &argv );

  MPI_Comm_size( MPI_COMM_WORLD, &size );
  
  /* determine the RANK of the current process [0:SIZE-1] */
  MPI_Comm_rank( MPI_COMM_WORLD, &rank );

  /* 
  ** make use of these values in our print statement
  ** note that we are assuming that all processes can
  ** write to the screen
  */
  printf("Hello, world: process %d of %d\n", rank, size);

  /* finialise the MPI enviroment */
  MPI_Finalize();

  /* and exit the program */
  return 0;
}
