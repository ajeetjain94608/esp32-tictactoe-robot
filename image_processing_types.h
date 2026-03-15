/*
 * Trial License - for use to evaluate programs for possible purchase as
 * an end-user only.
 *
 * ESP_process_image_types.h
 *
 * Code generation for function 'ESP_process_image'
 *
 */

#ifndef ESP_PROCESS_IMAGE_TYPES_H
#define ESP_PROCESS_IMAGE_TYPES_H

/* Include files */
#include "rtwtypes.h"

/* Type Definitions */
#ifndef struct_emxArray_char_T
#define struct_emxArray_char_T
struct emxArray_char_T {
  char *data;
  int *size;
  int allocatedSize;
  int numDimensions;
  boolean_T canFreeData;
};
#endif /* struct_emxArray_char_T */
#ifndef typedef_emxArray_char_T
#define typedef_emxArray_char_T
typedef struct emxArray_char_T emxArray_char_T;
#endif /* typedef_emxArray_char_T */

#endif
/* End of code generation (ESP_process_image_types.h) */
