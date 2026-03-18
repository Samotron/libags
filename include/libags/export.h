#ifndef LIBAGS_EXPORT_H
#define LIBAGS_EXPORT_H

#if defined(__cplusplus)
#define LIBAGS_EXTERN_C_BEGIN extern "C" {
#define LIBAGS_EXTERN_C_END }
#else
#define LIBAGS_EXTERN_C_BEGIN
#define LIBAGS_EXTERN_C_END
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(LIBAGS_BUILD_SHARED)
#    define LIBAGS_API __declspec(dllexport)
#  elif !defined(LIBAGS_STATIC)
#    define LIBAGS_API __declspec(dllimport)
#  else
#    define LIBAGS_API
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define LIBAGS_API __attribute__((visibility("default")))
#else
#  define LIBAGS_API
#endif

#endif
