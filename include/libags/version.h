#ifndef LIBAGS_VERSION_H
#define LIBAGS_VERSION_H

#include <stdint.h>

#include "libags/export.h"

#define AGS_VERSION_MAJOR 0
#define AGS_VERSION_MINOR 1
#define AGS_VERSION_PATCH 0

#define AGS_ABI_VERSION 1u

LIBAGS_EXTERN_C_BEGIN

LIBAGS_API int ags_version_major(void);
LIBAGS_API int ags_version_minor(void);
LIBAGS_API int ags_version_patch(void);
LIBAGS_API const char *ags_version_string(void);
LIBAGS_API uint32_t ags_abi_version(void);

LIBAGS_EXTERN_C_END

#endif
