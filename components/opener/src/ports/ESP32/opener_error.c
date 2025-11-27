/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "opener_error.h"

const int kErrorMessageBufferSize = 255;

int GetSocketErrorNumber(void) {
  return errno;
}

char* GetErrorMessage(int error_number) {
  char *error_message = malloc(kErrorMessageBufferSize);
  strerror_r(error_number, error_message, kErrorMessageBufferSize);
  return error_message;
}

void FreeErrorMessage(char *error_message) {
  free(error_message);
}

