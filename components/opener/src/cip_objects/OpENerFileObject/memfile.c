/*******************************************************************************
 * Memory-based FILE wrapper for embedded EDS file
 * Provides FILE-like interface for reading from memory buffer
 ******************************************************************************/

#include "memfile.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>  /* For ssize_t */

/* Ensure _GNU_SOURCE is defined for fopencookie */
#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

#include <stdio.h>

/* fopencookie might need stdio_ext.h on some systems, but newlib should have it in stdio.h */

/* Memory FILE structure */
typedef struct {
    const uint8_t *data;      /* Pointer to data */
    size_t size;              /* Total size */
    size_t position;          /* Current read position */
    int flags;                /* File flags */
} memfile_impl_t;

/* Custom FILE operations for memory files */
static ssize_t memfile_read(void *cookie, char *buf, size_t size);
static int memfile_seek(void *cookie, off_t *offset, int whence);
static int memfile_close_impl(void *cookie);

/* Cookie operations structure */
static cookie_io_functions_t memfile_ops = {
    .read = memfile_read,
    .write = NULL,  /* Read-only */
    .seek = memfile_seek,
    .close = memfile_close_impl
};

FILE *memfile_open(const uint8_t *data, size_t size, const char *mode) {
    if (data == NULL || size == 0) {
        errno = EINVAL;
        return NULL;
    }
    
    /* Only support read mode for now */
    if (mode == NULL || mode[0] != 'r') {
        errno = EINVAL;
        return NULL;
    }
    
    /* Allocate memory file structure */
    memfile_impl_t *mf = (memfile_impl_t *)malloc(sizeof(memfile_impl_t));
    if (mf == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    
    mf->data = data;
    mf->size = size;
    mf->position = 0;
    mf->flags = 0;
    
    /* Create FILE stream using fopencookie */
    /* Note: fopencookie is a GNU extension, may not be available in all newlib builds */
    FILE *fp = NULL;
    
    #ifdef __USE_GNU
        fp = fopencookie(mf, mode, memfile_ops);
    #else
        /* Try anyway - might work if _GNU_SOURCE is defined */
        fp = fopencookie(mf, mode, memfile_ops);
    #endif
    
    if (fp == NULL) {
        free(mf);
        errno = ENOSYS; /* Function not implemented */
        return NULL;
    }
    
    return fp;
}

int memfile_close(FILE *stream) {
    if (stream != NULL) {
        return fclose(stream);
    }
    return 0;
}

static ssize_t memfile_read(void *cookie, char *buf, size_t size) {
    memfile_impl_t *mf = (memfile_impl_t *)cookie;
    
    if (mf == NULL || buf == NULL) {
        return 0;
    }
    
    /* Calculate how much we can read */
    size_t available = mf->size - mf->position;
    size_t to_read = (size < available) ? size : available;
    
    if (to_read > 0) {
        memcpy(buf, &mf->data[mf->position], to_read);
        mf->position += to_read;
    }
    
    return (ssize_t)to_read;
}

static int memfile_seek(void *cookie, off_t *offset, int whence) {
    memfile_impl_t *mf = (memfile_impl_t *)cookie;
    
    if (mf == NULL || offset == NULL) {
        return -1;
    }
    
    off_t new_pos;
    
    switch (whence) {
        case SEEK_SET:
            new_pos = *offset;
            break;
        case SEEK_CUR:
            new_pos = (off_t)mf->position + *offset;
            break;
        case SEEK_END:
            new_pos = (off_t)mf->size + *offset;
            break;
        default:
            return -1;
    }
    
    /* Clamp to valid range */
    if (new_pos < 0) {
        new_pos = 0;
    } else if ((size_t)new_pos > mf->size) {
        new_pos = (off_t)mf->size;
    }
    
    mf->position = (size_t)new_pos;
    *offset = new_pos;
    
    return 0;
}

static int memfile_close_impl(void *cookie) {
    if (cookie != NULL) {
        free(cookie);
    }
    return 0;
}

