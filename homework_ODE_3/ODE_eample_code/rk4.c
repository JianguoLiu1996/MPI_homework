/****************************************************************/
/* Module : RK4.c 						*/
/* Section: 10.2						*/
/* Cheney-Kincaid, Numerical Mathematics and Computing, 5th ed, */
/* Brooks/Cole Publ. Co.                                        */
/* Copyright (c) 2003.  All rights reserved.                    */
/* For educational use with the Cheney-Kincaid textbook.        */
/* Absolutely no warranty implied or expressed.                 */
/* 								                               */
/* Description: Runge-Kutta 4th Order				           */
/** Solve the ODE: dx/dt = 2 + (x-t-1)^2, with x(1) = 2.        */
/** Note the exact solution is: x(t) = 1 + t + tan(t-1)          */
/*								                               */
/****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

double f(double t, double x); /* define prototype */


double f(double t, double x)
{
  return (2 + (x - t - 1) * (x - t - 1));
}


void rk4(double f(double, double), double t, double x, double h, int n)
{
  int k;
  double F1, F2, F3, F4, ta;
  double y;

  ta = t;

  for (k = 1; k <= n; k++)
  {
    F1 = h * f(t,x);
    F2 = h * f(t + 0.5 * h, x + F1 * 0.5);
    F3 = h * f(t + 0.5 * h, x + F2 * 0.5);
    F4 = h * f(t + h, x + F3);
    x += (F1 + 2 * (F2 + F3) + F4) / 6;

    t = (ta + k * h);
    y = 1 + t + tan(t-1);

    if(k==n) printf("k = %d t = %f x = %f y = %f error = %2.5e\n", k, t, x, y, fabs(y-x));
  }

}


void main()
{
  const int n = 256;
  const double a = 1.0, b = 1.5625;
  double h,t, x = 2.0;

  h = (b - a) / n;
  t = a;

  rk4 (f, t, x, h, n);

}
