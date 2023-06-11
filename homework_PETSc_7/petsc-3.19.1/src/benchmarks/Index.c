
#include <petscsys.h>
#include <petsctime.h>

extern int BlastCache(void);
extern int test1(void);
extern int test2(void);

int main(int argc,char **argv)
{

  PetscCall(PetscInitialize(&argc,&argv,0,0));
  PetscCall(test1());
  PetscCall(test2());
  PetscCall(PetscFinalize());
  return 0;
}

int test1(void)
{
  PetscLogDouble t1,t2;
  double         value;
  int            i,ierr,*z,*zi,intval;
  PetscScalar    *x,*y;
  PetscRandom    r;

  PetscCall(PetscRandomCreate(PETSC_COMM_SELF,&r));
  PetscCall(PetscRandomSetFromOptions(r));
  PetscCall(PetscMalloc1(20000,&x));
  PetscCall(PetscMalloc1(20000,&y));

  PetscCall(PetscMalloc1(2000,&z));
  PetscCall(PetscMalloc1(2000,&zi));

  /* Take care of paging effects */
  PetscCall(PetscTime(&t1));

  /* Form the random set of integers */
  for (i=0; i<2000; i++) {
    PetscCall(PetscRandomGetValue(r,&value));
    intval = (int)(value*20000.0);
    z[i]   = intval;
  }

  for (i=0; i<2000; i++) {
    PetscCall(PetscRandomGetValue(r,&value));
    intval = (int)(value*20000.0);
    zi[i]  = intval;
  }
  /* fprintf(stdout,"Done setup\n"); */

  PetscCall(BlastCache());

  PetscCall(PetscTime(&t1));
  for (i=0; i<2000; i++) x[i] = y[i];
  PetscCall(PetscTime(&t2));
  fprintf(stdout,"%-27s : %e sec\n","x[i] = y[i]",(t2-t1)/2000.0);

  PetscCall(BlastCache());

  PetscCall(PetscTime(&t1));
  for (i=0; i<500; i+=4) {
    x[i]   = y[z[i]];
    x[1+i] = y[z[1+i]];
    x[2+i] = y[z[2+i]];
    x[3+i] = y[z[3+i]];
  }
  PetscCall(PetscTime(&t2));
  fprintf(stdout,"%-27s : %e sec\n","x[i] = y[idx[i]] - unroll 4",(t2-t1)/2000.0);

  PetscCall(BlastCache());

  PetscCall(PetscTime(&t1));
  for (i=0; i<2000; i++) x[i] = y[z[i]];
  PetscCall(PetscTime(&t2));
  fprintf(stdout,"%-27s : %e sec\n","x[i] = y[idx[i]]",(t2-t1)/2000.0);

  PetscCall(BlastCache());

  PetscCall(PetscTime(&t1));
  for (i=0; i<1000; i+=2) {  x[i] = y[z[i]];  x[1+i] = y[z[1+i]]; }
  PetscCall(PetscTime(&t2));
  fprintf(stdout,"%-27s : %e sec\n","x[i] = y[idx[i]] - unroll 2",(t2-t1)/2000.0);

  PetscCall(BlastCache());

  PetscCall(PetscTime(&t1));
  for (i=0; i<2000; i++) x[z[i]] = y[i];
  PetscCall(PetscTime(&t2));
  fprintf(stdout,"%-27s : %e sec\n","x[z[i]] = y[i]",(t2-t1)/2000.0);

  PetscCall(BlastCache());

  PetscCall(PetscTime(&t1));
  for (i=0; i<2000; i++) x[z[i]] = y[zi[i]];
  PetscCall(PetscTime(&t2));
  fprintf(stdout,"%-27s : %e sec\n","x[z[i]] = y[zi[i]]",(t2-t1)/2000.0);

  PetscCall(PetscArraycpy(x,y,10));
  PetscCall(PetscArraycpy(z,zi,10));
  PetscCall(PetscFree(z));
  PetscCall(PetscFree(zi));
  PetscCall(PetscFree(x));
  PetscCall(PetscFree(y));
  PetscCall(PetscRandomDestroy(&r));
  PetscFunctionReturn(PETSC_SUCCESS);
}

int test2(void)
{
  PetscLogDouble t1,t2;
  double         value;
  int            i,ierr,z[20000],zi[20000],intval,tmp;
  PetscScalar    x[20000],y[20000];
  PetscRandom    r;

  PetscCall(PetscRandomCreate(PETSC_COMM_SELF,&r));
  PetscCall(PetscRandomSetFromOptions(r));

  /* Take care of paging effects */
  PetscCall(PetscTime(&t1));

  for (i=0; i<20000; i++) {
    x[i]  = i;
    y[i]  = i;
    z[i]  = i;
    zi[i] = i;
  }

  /* Form the random set of integers */
  for (i=0; i<20000; i++) {
    PetscCall(PetscRandomGetValue(r,&value));
    intval    = (int)(value*20000.0);
    tmp       = z[i];
    z[i]      = z[intval];
    z[intval] = tmp;
  }

  for (i=0; i<20000; i++) {
    PetscCall(PetscRandomGetValue(r,&value));
    intval     = (int)(value*20000.0);
    tmp        = zi[i];
    zi[i]      = zi[intval];
    zi[intval] = tmp;
  }
  /* fprintf(stdout,"Done setup\n"); */

  /* PetscCall(BlastCache()); */

  PetscCall(PetscTime(&t1));
  for (i=0; i<2000; i++) x[i] = y[i];
  PetscCall(PetscTime(&t2));
  fprintf(stdout,"%-27s : %e sec\n","x[i] = y[i]",(t2-t1)/2000.0);

  /* PetscCall(BlastCache()); */

  PetscCall(PetscTime(&t1));
  for (i=0; i<2000; i++) y[i] = x[z[i]];
  PetscCall(PetscTime(&t2));
  fprintf(stdout,"%-27s : %e sec\n","x[i] = y[idx[i]]",(t2-t1)/2000.0);

  /* PetscCall(BlastCache()); */

  PetscCall(PetscTime(&t1));
  for (i=0; i<2000; i++) x[z[i]] = y[i];
  PetscCall(PetscTime(&t2));
  fprintf(stdout,"%-27s : %e sec\n","x[z[i]] = y[i]",(t2-t1)/2000.0);

  /* PetscCall(BlastCache()); */

  PetscCall(PetscTime(&t1));
  for (i=0; i<2000; i++) y[z[i]] = x[zi[i]];
  PetscCall(PetscTime(&t2));
  fprintf(stdout,"%-27s : %e sec\n","x[z[i]] = y[zi[i]]",(t2-t1)/2000.0);

  PetscCall(PetscRandomDestroy(&r));
  PetscFunctionReturn(PETSC_SUCCESS);
}

int BlastCache(void)
{
  int         i,ierr,n = 1000000;
  PetscScalar *x,*y,*z,*a,*b;

  PetscCall(PetscMalloc1(5*n,&x));
  y    = x + n;
  z    = y + n;
  a    = z + n;
  b    = a + n;

  for (i=0; i<n; i++) {
    a[i] = (PetscScalar) i;
    y[i] = (PetscScalar) i;
    z[i] = (PetscScalar) i;
    b[i] = (PetscScalar) i;
    x[i] = (PetscScalar) i;
  }

  for (i=0; i<n; i++) a[i] = 3.0*x[i] + 2.0*y[i] + 3.3*z[i] - 25.*b[i];
  for (i=0; i<n; i++) b[i] = 3.0*x[i] + 2.0*y[i] + 3.3*a[i] - 25.*b[i];
  for (i=0; i<n; i++) z[i] = 3.0*x[i] + 2.0*y[i] + 3.3*a[i] - 25.*b[i];
  PetscCall(PetscFree(x));
  PetscFunctionReturn(PETSC_SUCCESS);
}
