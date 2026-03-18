add_executable(libags_tests test_main.c)

target_link_libraries(libags_tests PRIVATE ags_static)

if(MSVC)
  target_compile_options(libags_tests PRIVATE /W4 /WX)
else()
  target_compile_options(libags_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME libags_tests COMMAND libags_tests)
set_tests_properties(libags_tests PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
