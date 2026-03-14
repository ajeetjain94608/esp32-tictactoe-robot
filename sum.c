/*
 * Trial License - for use to evaluate programs for possible purchase as
 * an end-user only.
 *
 * sum.c
 *
 * Code generation for function 'sum'
 *
 */

/* Include files */
#include "sum.h"
#include <string.h>

/* Function Definitions */
void sum(const unsigned char x_data[], const int x_size[2], double y_data[],
         int y_size[2])
{
  int k;
  int vlen;
  int xi;
  vlen = x_size[0];
  if ((x_size[0] == 0) || (x_size[1] == 0)) {
    y_size[0] = 1;
    y_size[1] = (unsigned char)x_size[1];
    vlen = (unsigned char)x_size[1];
    if (vlen - 1 >= 0) {
      memset(&y_data[0], 0, (unsigned int)vlen * sizeof(double));
    }
  } else {
    int i;
    i = x_size[1];
    y_size[0] = 1;
    y_size[1] = x_size[1];
    for (xi = 0; xi < i; xi++) {
      int xpageoffset;
      xpageoffset = xi * x_size[0];
      y_data[xi] = x_data[xpageoffset];
      for (k = 2; k <= vlen; k++) {
        y_data[xi] += (double)x_data[(xpageoffset + k) - 1];
      }
    }
  }
}

/* End of code generation (sum.c) */
