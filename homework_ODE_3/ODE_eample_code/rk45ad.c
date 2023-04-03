/****************************************************************/
/* Module : rk45ad.c 						*/
/* Section: 10.3						*/
/* Cheney-Kincaid, Numerical Mathematics and Computing, 5th ed, */
/* Brooks/Cole Publ. Co.                                        */
/* Copyright (c) 2003.  All rights reserved.                    */
/* For educational use with the Cheney-Kincaid textbook.        */
/* Absolutely no warranty implied or expressed.                 */
/* 								*/
/* Description: the adaptive rk45 method     					*/
/** Solve the ODE: dx/dt = 2 + (x-t-1)^2, with x(1) = 2.        */
/** Note the exact solution is: x(t) = 1 + t + tan(t-1)          */
/*								                               */
/****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define sign(fp) (( (fp) >= (0)) ? (1) : (-1))

#ifndef max
#define max(a,b) (( (a) >= (b)) ? (a) : (b))
#endif

/* define prototypes */

float f(float, float);
void rk45 (float f(float, float), float *, float *, float, float *);
int rk45ad (float f(float, float), float *, float *, float, float, int, float,
             float, float, float, int);

float f(float t, float x)
{
  float a, b, c;
  a = 3; b = 5; c = 0.2;
  return a + b*sin(t) + c*x;
}


void rk45(float f(float, float), float* t, float* x, float h, float* epsilon)
{

  float c20 = 0.25, c21 = 0.25;
  float c30 = 0.375, c31 = 0.09375, c32 = 0.28125;
  float c40,c41, c42,c43;
  float c51, c52 = -8.0, c53, c54;
  float c60 = 0.5, c61, c62 = 2, c63, c64;
  float c65 = -0.275;
  float a1, a2 = 0, a3, a4, a5 = -0.2;
  float b1, b2 = 0, b3, b4, b5= -0.18, b6;
  float F1, F2, F3, F4, F5, F6, x4;

  c40 = (float) 12/ (float) 13;
  c41 = (float) 1932/(float) 2197;
  c42 = (float) -7200/(float) 2197;
  c43 = (float) 7296/(float) 2197;
  c51 = c53 = (float) 439/ (float) 216;
  c54 = (float) -845/(float) 4104;
  c61 = (float) -8/(float) 27;
  c63 = (float) -3544/(float) 2565;
  c64 = (float) 1859/(float) 4104;
  a1 = (float) 25/(float) 216;
  a3 = (float) 1408/(float) 2565;
  a4 = (float) 2197/(float) 4104;
  b1 = (float) 16/(float) 135;
  b3 = (float) 6656/(float) 12825;
  b4 = (float) 28561/(float) 56430;
  b6 = (float) 2/(float) 55;


  F1 = h * f(*t, *x);
  F2 = h * f(*t + c20 * h, *x + c21 * F1);
  F3 = h * f(*t + c30 * h, *x + c31 * F1 + c32 * F2);
  F4 = h * f(*t + c40 * h, *x + c41 * F1 + c42 * F2 + c43 * F3);
  F5 = h * f(*t + h, *x + c51 * F1 + c52 * F2 + c53 * F3 + c54 * F4 );
  F6 = h * f(*t + c60 * h, *x + c61 * F1 + c62 * F2 + c63 * F3 + c64 * F4 + c65 * F5);

  x4 = *x + a1 * F1 + a3 * F3 + a4 * F4 + a5 * F5;
  *x += b1 * F1 + b3 * F3 + b4 * F4 + b5 * F5 + b6 * F6;
  *t += h;
  *epsilon = fabs(*x - x4);

}

int rk45ad (float f(float, float), float *pt, float *px, float h, float tb,
           int itmax, float emin, float emax, float hmin, float hmax, int iflag)
{
  float delta = 5.0e-6;
  float d, xsave, tsave;
  int k;
  FILE *fptr;

  fptr = fopen("output-rk45ad.txt","w");
  printf ("Write the step results to file output-rk45.txt \n");
  //fprintf(fptr, "t             x  \n");
  printf ("n    h		t 		x\n");
  printf ("0 %14e %14e %14e\n", h, *pt, *px);

  iflag = 1;
  k = 0;

  while (k <= itmax)
  {
    k++;
    float epsilon=0;

    if (fabs(h) < hmin)
      h = sign(h) * hmin;
    if (fabs(h) > hmax)
      h = sign(h) * hmax;

    d = fabs(tb - *pt);

    if (d <= fabs(h) )
    {
      iflag = 0;

      if (d <= ( delta * max( fabs(tb), fabs(*pt) )))
	    return(1);
      h = sign(h) * d;
    }

    xsave = *px;
    tsave = *pt;

    rk45(f, pt, px, h, &epsilon);

    fprintf(fptr, "%14e %14e \n", *pt, *px);
    if(fabs(*pt-tb)<=h)
    printf("k=%d h=%14e t=%14e x=%14e epsilon=%14e\n", k, h, *pt, *px, epsilon);

    if (iflag == 0)
      return(1);

    if ( epsilon < emin)
      h *= 2;

    if ( epsilon > emax)
    {
      h *= 0.5;
      *px = xsave;
      *pt = tsave;
      k--;
    }

  }
  fclose(fptr);
  return(0);
}


void main()
{
  const int itmax = 10000;
  float t = 0.0, x = 0.0, h = 0.01, tb = 10;
  float epsmin = 1.0e-8, epsmax = 1.0e-5, hmin = 1.0e-6, hmax = 1.0;
  int iflag;

  rk45ad (f, &t, &x, h, tb, itmax, epsmin, epsmax, hmin, hmax, iflag);

}
