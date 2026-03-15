/*
 * Trial License - for use to evaluate programs for possible purchase as
 * an end-user only.
 *
 * ESP_process_image_emxutil.h
 *
 * Code generation for function 'ESP_process_image_emxutil'
 *
 */

#ifndef ESP_PROCESS_IMAGE_EMXUTIL_H
#define ESP_PROCESS_IMAGE_EMXUTIL_H

/* Include files */
#include "image_processing_types.h"
#include "rtwtypes.h"
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Function Declarations */
extern void emxEnsureCapacity_char_T(emxArray_char_T *emxArray, int oldNumel);

extern void emxFree_char_T(emxArray_char_T **pEmxArray);

extern void emxInit_char_T(emxArray_char_T **pEmxArray);

#ifdef __cplusplus
}
#endif

#endif
/* End of code generation (ESP_process_image_emxutil.h) */
