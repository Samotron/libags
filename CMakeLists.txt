cmake_minimum_required(VERSION 3.20)

project(
  libags
  VERSION 0.1.0
  DESCRIPTION "C library for working with AGS data"
  LANGUAGES C
)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(LIBAGS_BUILD_TESTS "Build libags tests" ${BUILD_TESTING})

set(LIBAGS_PUBLIC_HEADERS
  include/libags/allocator.h
  include/libags/dictionary.h
  include/libags/document.h
  include/libags/export.h
  include/libags/ffi.h
  include/libags/libags.h
  include/libags/merge.h
  include/libags/status.h
  include/libags/tabular.h
  include/libags/validate.h
  include/libags/version.h
)

set(LIBAGS_SOURCES
  src/allocator.c
  src/dictionary.c
  src/dictionary_bundle_data.c
  src/document.c
  src/ffi.c
  src/merge.c
  src/parse.c
  src/serialize.c
  src/status.c
  src/tabular.c
  src/validate.c
  src/validate_dictionary.c
  src/version.c
)

add_library(ags_static STATIC ${LIBAGS_PUBLIC_HEADERS} ${LIBAGS_SOURCES})
add_library(ags_shared SHARED ${LIBAGS_PUBLIC_HEADERS} ${LIBAGS_SOURCES})

set_target_properties(ags_static PROPERTIES
  OUTPUT_NAME ags
)

set_target_properties(ags_shared PROPERTIES
  OUTPUT_NAME ags
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
)

if(WIN32)
  set_target_properties(ags_static PROPERTIES
    OUTPUT_NAME ags_static
  )
endif()

target_compile_definitions(ags_static PUBLIC LIBAGS_STATIC=1)
target_compile_definitions(ags_shared PRIVATE LIBAGS_BUILD_SHARED=1)

target_include_directories(ags_static
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(ags_shared
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
  target_compile_options(ags_static PRIVATE /W4 /WX)
  target_compile_options(ags_shared PRIVATE /W4 /WX)
else()
  target_compile_options(ags_static PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(ags_shared PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(libags::static ALIAS ags_static)
add_library(libags::shared ALIAS ags_shared)

install(
  TARGETS ags_static ags_shared
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  FILES LICENSE
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/libags
)

if(LIBAGS_BUILD_TESTS)
  add_subdirectory(tests)
endif()
