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
  //return(3*x/t+9/2*t-13);
   return (3*x/t+9/2*t-13);
}


void main()
{
  const int n = 100;
  const float a = 3, b = 0.5;
  int k;
  float h,t, x = 6, y;

  h = (b - a) / n;
  t = a;

  for (k = 1; k <= n; k++)
  {
    x += h * f(t, x);
    t += h;
    y = t*t*t-9/2*t*t+13/2*t;
    printf("k = %d t = %f x = %f y = %f error=%f\n", k, t, x, y, fabs(y-x));
  }
}









