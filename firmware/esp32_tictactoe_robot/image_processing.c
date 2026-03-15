/*
 * Trial License - for use to evaluate programs for possible purchase as
 * an end-user only.
 *
 * ESP_process_image.c
 *
 * Code generation for function 'ESP_process_image'
 *
 */

/* Include files */
#include "image_processing.h"
#include "image_emx_utils.h"
#include "image_processing_types.h"
#include "math_utils.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

/* Function Declarations */
static double rt_roundd_snf(double u);

/* Function Definitions */
static double rt_roundd_snf(double u)
{
  double y;
  if (fabs(u) < 4.503599627370496E+15) {
    if (u >= 0.5) {
      y = floor(u + 0.5);
    } else if (u > -0.5) {
      y = u * 0.0;
    } else {
      y = ceil(u - 0.5);
    }
  } else {
    y = u;
  }
  return y;
}

void ESP_process_image(const unsigned short data[57600], char boardState[5][5])
{
  static const double rectangles[100] = {
      25.6836,  67.655,   110.0079, 153.5056, 196.6216, 25.6836,  68.4181,
      111.1526, 152.7424, 196.2401, 26.0652,  68.7997,  111.1526, 153.5056,
      196.6216, 26.0652,  68.7997,  109.2448, 152.7424, 196.6216, 25.3021,
      67.2734,  110.0079, 152.3609, 195.0954, 27.2099,  27.973,   27.5914,
      27.2099,  26.0652,  71.8521,  71.4706,  71.8521,  71.8521,  70.7075,
      113.8235, 114.2051, 114.5866, 114.5866, 114.5866, 156.1765, 157.7027,
      157.3211, 158.0843, 158.8474, 200.4372, 200.5,    200.4372, 200.5,
      200.5,    40.0,     40.0,     40.0,     40.0,     40.0,     40.0,
      40.0,     40.0,     40.0,     40.0,     40.0,     40.0,     40.0,
      40.0,     40.0,     40.0,     40.0,     40.0,     40.0,     40.0,
      40.0,     40.0,     40.0,     40.0,     40.0,     40.0,     40.0,
      40.0,     40.0,     40.0,     40.0,     40.0,     40.0,     40.0,
      40.0,     40.0,     40.0,     40.0,     40.0,     40.0,     40.0,
      40.0,     40.0,     40.0,     40.0,     40.0,     40.0,     40.0,
      40.0,     40.0};
  static const char names[3] = {'O', ' ', 'X'};
  static unsigned char rgb[172800];
  emxArray_char_T *symbol;
  double x_data[240];
  int rgb_size[2];
  int b_i;
  int colIdx2;
  int i;
  int i1;
  int rowIdx2;
  unsigned short b_data[57600];
  unsigned short b_y;
  char *symbol_data;
  /*  Read the raw image data into a vector */
  /* fid = fopen(imageFile, 'r'); */
  /* data = fread(fid, inf, 'uint16'); */
  /* fclose(fid); */
  /*  Reshape the data into an M-by-N matrix */
  /*  Image height */
  /*  Image width */
  for (i = 0; i < 240; i++) {
    for (i1 = 0; i1 < 240; i1++) {
      b_data[i1 + 240 * i] = data[i + 240 * i1];
    }
  }
  for (colIdx2 = 0; colIdx2 < 57600; colIdx2++) {
    unsigned char y[2];
    unsigned char xtmp;
    memcpy((void *)&y[0], (void *)&b_data[colIdx2],
           (unsigned int)((size_t)2 * sizeof(unsigned char)));
    xtmp = y[0];
    y[0] = y[1];
    y[1] = xtmp;
    memcpy((void *)&b_y, (void *)&y[0],
           (unsigned int)((size_t)1 * sizeof(unsigned short)));
    b_data[colIdx2] = b_y;
  }
  /*  Convert the RGB565 data to RGB888 format */
  for (colIdx2 = 0; colIdx2 < 57600; colIdx2++) {
    b_y = b_data[colIdx2];
    rgb[colIdx2] =
        (unsigned char)((unsigned int)(unsigned short)(b_y & 63488) >> 8);
    rgb[colIdx2 + 57600] =
        (unsigned char)((unsigned int)(unsigned short)(b_y & 2016) >> 3);
    rgb[colIdx2 + 115200] = (unsigned char)((unsigned short)(b_y & 31) << 3);
  }
  /*  Define rectangles */
  emxInit_char_T(&symbol);
  symbol_data = symbol->data;
  symbol->size[0] = 1;
  symbol->size[1] = 0;
  for (b_i = 0; b_i < 25; b_i++) {
    double b_y_tmp;
    double c_y;
    double y_tmp;
    int x_size[2];
    int b_loop_ub_tmp;
    int loop_ub_tmp;
    unsigned char rgb_data[57600];
    /*  Slice the image */
    y_tmp = (rectangles[b_i + 25] - 1.0) + 1.0;
    rowIdx2 = (int)rt_roundd_snf(y_tmp + rectangles[b_i + 75]);
    b_y_tmp = (rectangles[b_i] - 1.0) + 1.0;
    colIdx2 = (int)rt_roundd_snf(b_y_tmp + rectangles[b_i + 50]);
    if (rowIdx2 > 240) {
      rowIdx2 = 240;
    }
    if (colIdx2 > 240) {
      colIdx2 = 240;
    }
    i = (int)rt_roundd_snf(y_tmp);
    if (i > rowIdx2) {
      i = 1;
      rowIdx2 = 0;
    }
    i1 = (int)rt_roundd_snf(b_y_tmp);
    if (i1 > colIdx2) {
      i1 = 1;
      colIdx2 = 0;
    }
    loop_ub_tmp = rowIdx2 - i;
    b_loop_ub_tmp = colIdx2 - i1;
    rgb_size[0] = loop_ub_tmp + 1;
    rgb_size[1] = b_loop_ub_tmp + 1;
    for (rowIdx2 = 0; rowIdx2 <= b_loop_ub_tmp; rowIdx2++) {
      for (colIdx2 = 0; colIdx2 <= loop_ub_tmp; colIdx2++) {
        rgb_data[colIdx2 + (loop_ub_tmp + 1) * rowIdx2] =
            rgb[((i + colIdx2) + 240 * ((i1 + rowIdx2) - 1)) - 1];
      }
    }
    sum(rgb_data, rgb_size, x_data, x_size);
    rowIdx2 = x_size[1];
    if (x_size[1] == 0) {
      y_tmp = 0.0;
    } else {
      y_tmp = x_data[0];
      for (colIdx2 = 2; colIdx2 <= rowIdx2; colIdx2++) {
        y_tmp += x_data[colIdx2 - 1];
      }
    }
    rgb_size[0] = loop_ub_tmp + 1;
    rgb_size[1] = b_loop_ub_tmp + 1;
    for (rowIdx2 = 0; rowIdx2 <= b_loop_ub_tmp; rowIdx2++) {
      for (colIdx2 = 0; colIdx2 <= loop_ub_tmp; colIdx2++) {
        rgb_data[colIdx2 + (loop_ub_tmp + 1) * rowIdx2] =
            rgb[((i + colIdx2) + 240 * ((i1 + rowIdx2) - 1)) + 57599];
      }
    }
    sum(rgb_data, rgb_size, x_data, x_size);
    rowIdx2 = x_size[1];
    if (x_size[1] == 0) {
      b_y_tmp = 0.0;
    } else {
      b_y_tmp = x_data[0];
      for (colIdx2 = 2; colIdx2 <= rowIdx2; colIdx2++) {
        b_y_tmp += x_data[colIdx2 - 1];
      }
    }
    rgb_size[0] = loop_ub_tmp + 1;
    rgb_size[1] = b_loop_ub_tmp + 1;
    for (rowIdx2 = 0; rowIdx2 <= b_loop_ub_tmp; rowIdx2++) {
      for (colIdx2 = 0; colIdx2 <= loop_ub_tmp; colIdx2++) {
        rgb_data[colIdx2 + (loop_ub_tmp + 1) * rowIdx2] =
            rgb[((i + colIdx2) + 240 * ((i1 + rowIdx2) - 1)) + 115199];
      }
    }
    sum(rgb_data, rgb_size, x_data, x_size);
    rowIdx2 = x_size[1];
    if (x_size[1] == 0) {
      c_y = 0.0;
    } else {
      c_y = x_data[0];
      for (colIdx2 = 2; colIdx2 <= rowIdx2; colIdx2++) {
        c_y += x_data[colIdx2 - 1];
      }
    }
    rowIdx2 = -1;
    if (y_tmp < b_y_tmp) {
      y_tmp = b_y_tmp;
      rowIdx2 = 0;
    }
    if (y_tmp < c_y) {
      rowIdx2 = 1;
    }
    i = symbol->size[1];
    i1 = symbol->size[0] * symbol->size[1];
    symbol->size[1]++;
    emxEnsureCapacity_char_T(symbol, i1);
    symbol_data = symbol->data;
    symbol_data[i] = names[rowIdx2 + 1];
  }
  for (b_i = 0; b_i < 5; b_i++) {
    for (rowIdx2 = 0; rowIdx2 < 5; rowIdx2++) {
      boardState[rowIdx2][4-i] = symbol_data[b_i * 5 + rowIdx2];
    }
  }
  emxFree_char_T(&symbol);
}

/* End of code generation (ESP_process_image.c) */
