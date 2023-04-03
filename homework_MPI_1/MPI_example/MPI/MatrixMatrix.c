    #include <stdio.h>
    #include <stdlib.h>
    #include <mpi.h>
    #include <time.h>
    int main(int argc, char *argv[])
    {
      double start, stop;
      int i, j, k;
      int *a, *b, *c, *buffer, *ans;
      int n = 10;
      int rank, size, line;

      MPI_Init(&argc, &argv);    //MPI Initialize
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);      //obtain the rank id
      MPI_Comm_size(MPI_COMM_WORLD, &size);      //obtain the number of processes
      if (argc < 2) {
        if(!rank)printf("Usage: %s  <size> (set the size of the matrix, default is 10)\n", argv[0]);
        n = 10;
      }else{
        sscanf(argv[1],"%d",&n);
      }

      line = n / size;      // divide the data into data chunk
      a = (int*)malloc(sizeof(int) * n * n);
      b = (int*)malloc(sizeof(int) * n * n);
      c = (int*)malloc(sizeof(int) * n * n);
      buffer = (int*)malloc(sizeof(int) * n * line);
      ans = (int*)malloc(sizeof(int) * n * line);      // used to store results
      start = MPI_Wtime();
      if (rank == 0){      // master process
          for(i = 0; i < n; i++){ // assign value
            for(j = 0; j < n; j++){
              a[i * n + j] = i;
            }
          }

          for(i = 0; i < n; i++){
            for(j = 0; j < n; j++){
              b[i * n + j] = 2;
            }
          }

          for (i = 1; i < size; i++){      // send b to each process
            MPI_Send(b, n * n, MPI_INT, i, 0, MPI_COMM_WORLD);
          }

          for (i = 1; i < size; i++){    // send a chunk of a to each process
             MPI_Send(a + (i - 1) * line * n, n * line, MPI_INT, i, 1, MPI_COMM_WORLD);
          }

          for (k = 1; k < size; k++){  // receive the answers
            MPI_Recv(ans, line * n, MPI_INT, k, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (i = 0; i < line; i++){ // copy the results from ans to c
              for (j = 0; j < n; j++){
                c[((k - 1) * line + i) * n + j] = ans[i * n + j];
              }
            }
          }
          for (i = (size - 1) * line; i < n; i++){     // compute the left in a
            for (j = 0; j < n; j++){
              int temp = 0;
              for (k = 0; k < n; k++){
                temp += a[i * n + k] * b[k * n + j];
              }
              c[i * n + j] = temp;
            }
          }
          #if(0)
          printf("The matrix a:\n");
          for(i = 0; i < n; i++){
            for(j = 0; j < n; j++){
              printf("%5d ", a[i * n + j]);
            }
            printf("\n");
          }
          printf("The matrix b:\n");
          for(i = 0; i < n; i++){
            for(j = 0; j < n; j++){
              printf("%5d ", b[i * n + j]);
            }
            printf("\n");
          }
          printf("The matrix c=a*b:\n");
          for(i = 0; i < n; i++){
            for(j = 0; j < n; j++){
              printf("%5d ", c[i * n + j]);
            }
            printf("\n");
          }
          #endif
      }else{
        MPI_Recv(b, n * n, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(buffer, n * line, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // compute the results
        for (i = 0; i < line; i++){
          for (j = 0; j < n; j++){
            int temp = 0;
            for(k = 0; k < n; k++){
              temp += buffer[i * n + k] * b[k * n + j];
            }
            ans[i * n + j] = temp;
          }
        }
        MPI_Send(ans, line * n, MPI_INT, 0, 3, MPI_COMM_WORLD);      // send back the computed result
    }
    stop = MPI_Wtime();
    double ctime_local=0;
    double ctime = 0;
    ctime_local = stop-start;
    MPI_Barrier(MPI_COMM_WORLD);
    printf("rank:%d Compute time:%lf s\n", rank, ctime_local);
    MPI_Reduce(&ctime_local, &ctime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if(!rank)printf("The max compute time:%lf s\n", ctime);

    free(a);
    free(b);
    free(c);
    free(buffer);
    free(ans);

    MPI_Finalize();
    return 0;
  }
