#include <stdlib.h>

#include "libags/allocator.h"

static void *ags_default_malloc_impl(void *user_data, size_t size) {
  (void)user_data;
  return malloc(size);
}

static void *ags_default_realloc_impl(void *user_data, void *ptr, size_t size) {
  (void)user_data;
  return realloc(ptr, size);
}

static void ags_default_free_impl(void *user_data, void *ptr) {
  (void)user_data;
  free(ptr);
}

const ags_allocator *ags_default_allocator(void) {
  static const ags_allocator allocator = {
    ags_default_malloc_impl,
    ags_default_realloc_impl,
    ags_default_free_impl,
    NULL
  };

  return &allocator;
}

int ags_allocator_is_valid(const ags_allocator *allocator) {
  if (allocator == NULL) {
    return 0;
  }

  if (allocator->malloc_fn == NULL) {
    return 0;
  }

  if (allocator->realloc_fn == NULL) {
    return 0;
  }

  if (allocator->free_fn == NULL) {
    return 0;
  }

  return 1;
}
