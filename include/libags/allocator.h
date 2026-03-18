#ifndef LIBAGS_ALLOCATOR_H
#define LIBAGS_ALLOCATOR_H

#include <stddef.h>

#include "libags/export.h"

LIBAGS_EXTERN_C_BEGIN

typedef void *(*ags_malloc_fn)(void *user_data, size_t size);
typedef void *(*ags_realloc_fn)(void *user_data, void *ptr, size_t size);
typedef void (*ags_free_fn)(void *user_data, void *ptr);

typedef struct ags_allocator {
  ags_malloc_fn malloc_fn;
  ags_realloc_fn realloc_fn;
  ags_free_fn free_fn;
  void *user_data;
} ags_allocator;

LIBAGS_API const ags_allocator *ags_default_allocator(void);
LIBAGS_API int ags_allocator_is_valid(const ags_allocator *allocator);

LIBAGS_EXTERN_C_END

#endif
