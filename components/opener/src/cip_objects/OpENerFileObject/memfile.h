/*******************************************************************************
 * Memory-based FILE wrapper for embedded EDS file
 * Provides FILE-like interface for reading from memory buffer
 ******************************************************************************/

#ifndef MEMFILE_H_
#define MEMFILE_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

/* Memory FILE structure */
typedef struct {
    const uint8_t *data;      /* Pointer to data */
    size_t size;              /* Total size */
    size_t position;          /* Current read position */
} memfile_t;

/* Create a memory FILE handle */
FILE *memfile_open(const uint8_t *data, size_t size, const char *mode);

/* Close memory FILE handle */
int memfile_close(FILE *stream);

#endif /* MEMFILE_H_ */

