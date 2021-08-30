# You don't need to compile any library to use minijson_reader in your project:
# just include the minijson_reader.hpp header anywhere you need, and you're ready to go.

# Use this cmake project to build and run the unit tests (you will need Google Test):
#   $ mkdir build
#   $ cd build
#   $ cmake ..
#   $ make
#   $ ctest
#   $ make install

cmake_minimum_required(VERSION 3.13...3.21)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(minijson_reader LANGUAGES CXX)

# ---- dependency ----
find_package(GTest REQUIRED)

option(CMAKE_CXX_EXTENSIONS "Default value for CXX_EXTENSIONS property of targets." off)
set(CMAKE_CXX_STANDARD_REQUIRED 17 CACHE STRING "the requiered c++ standard")

# Where to put all the RUNTIME targets when built.  This variable is used to
# initialize the RUNTIME_OUTPUT_DIRECTORY property on all the targets.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

add_executable(minijson_example minijson_reader.hpp minijson_example.cpp)
set_property(TARGET minijson_example PROPERTY CXX_STANDARD ${CMAKE_CXX_STANDARD_REQUIRED})
target_compile_features(minijson_example PRIVATE cxx_lambdas)

add_executable(minijson_parser minijson_reader.hpp minijson_parser.cpp)
set_property(TARGET minijson_parser PROPERTY CXX_STANDARD ${CMAKE_CXX_STANDARD_REQUIRED})

add_executable(${PROJECT_NAME}_tests minijson_reader.hpp minijson_reader_tests.cpp)
set_property(TARGET ${PROJECT_NAME}_tests PROPERTY CXX_STANDARD ${CMAKE_CXX_STANDARD_REQUIRED})
target_link_libraries(${PROJECT_NAME}_tests GTest::gtest_main)

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(${PROJECT_NAME}_tests PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_SIMULATE_ID MATCHES "MSVC")
  target_compile_options(${PROJECT_NAME}_tests PRIVATE /W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(${PROJECT_NAME}_tests PRIVATE -Wall -Wextra -Werror -Wno-shadow -Wno-unknown-pragmas)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL 10.0)
    target_compile_options(${PROJECT_NAME}_tests PRIVATE -Wdeprecated-copy-dtor -Wnewline-eof)
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  target_compile_options(${PROJECT_NAME}_tests PRIVATE /W4 /WX)
endif()

# ---- code coverage ----

if(NOT MSVC)
  option(ENABLE_TEST_COVERAGE "Enable test coverage" ON)
  if(ENABLE_TEST_COVERAGE AND "${CMAKE_BUILD_TYPE}" STREQUAL Debug)
    foreach(file ${PROJECT_NAME}_tests minijson_example)
      target_compile_options(${file} PUBLIC -O0 -g -fprofile-arcs -ftest-coverage)
      target_link_options(${file} PUBLIC -fprofile-arcs -ftest-coverage)
    endforeach()
  endif()
endif()

# ---- ctest ----

enable_testing()
add_test(NAME all_tests COMMAND ${PROJECT_NAME}_tests)
add_test(NAME minijson_example COMMAND minijson_example)

# ---- install ----

install(FILES minijson_reader.hpp DESTINATION include)

