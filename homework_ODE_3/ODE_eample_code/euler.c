/****************************************************************/
/* Module : euler.c 						*/
/* Section: 10.1						*/
/* Cheney-Kincaid, Numerical Mathematics and Computing, 5th ed, */
/* Brooks/Cole Publ. Co.                                        */
/* Copyright (c) 2003.  All rights reserved.                    */
/* For educational use with the Cheney-Kincaid textbook.        */
/* Absolutely no warranty implied or expressed.                 */
/* 								                               */
/* Description: Euler's method  				                   */
/** Solve the ODE: dx/dt = 1 + x^2 + t^3, with x(1) = -4.        */
/** Note the exact result at point 2 is:                         */
/** x (2) Å 4.37121005224969227234569                            */
/*								*/
/****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

float f(float t, float x); /*define prototype */

float f(float t, float x)
{
  //return(1 + x * x + t * t * t);
   return (2 + (x - t - 1) * (x - t - 1));
}


void main()
{
  const int n = 400;
  const float a = 1.0, b = 2.0;
  int k;
  float h,t, x = -4.0, y;

  x = 1 + 1 + tan(1-1);
  h = (b - a) / n;
  t = a;
  printf("0 t = %f x = %f\n", t, x);

  for (k = 1; k <= n; k++)
  {
    x += h * f(t, x);
    t += h;
    y = 1 + t + tan(t-1);
    printf("k = %d t = %f x = %f y = %f error=%f\n", k, t, x, y, fabs(y-x));
  }
}









