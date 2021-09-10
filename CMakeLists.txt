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

option(CMAKE_CXX_EXTENSIONS "Default value for CXX_EXTENSIONS property of targets." NO)
set(CMAKE_CXX_STANDARD 17 CACHE STRING "The used c++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED On CACHE STRING "The c++ standard is requrired")

# Where to put all the RUNTIME targets when built.  This variable is used to
# initialize the RUNTIME_OUTPUT_DIRECTORY property on all the targets.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# ---- dependency ----
#XXX find_package(GTest 1.11 REQUIRED)
include(cmake/CPM.cmake)

CPMAddPackage(
  NAME googletest
  GITHUB_REPOSITORY google/googletest
  GIT_TAG release-1.11.0
  VERSION 1.11.0
  OPTIONS "BUILD_GMOCK OFF" "gtest_force_shared_crt"
)

CPMAddPackage(
  NAME minijson_writer
  GITHUB_REPOSITORY ClausKlein/minijson_writer
  GIT_TAG c9553c7687e1f2151ceb0cee04fc878354f24431
)

# ---- Create library ----

# Note: for header-only libraries change all PUBLIC flags to INTERFACE
add_library(${PROJECT_NAME} INTERFACE minijson_reader.hpp)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD ${CMAKE_CXX_STANDARD})
target_include_directories(
  ${PROJECT_NAME} BEFORE INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                   $<INSTALL_INTERFACE:include>
)
# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(${PROJECT_NAME} INTERFACE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

add_library(minijson::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# ---- Create executables ----

add_executable(minijson_example minijson_reader.hpp minijson_example.cpp)
target_compile_features(minijson_example PRIVATE cxx_lambdas)

add_executable(sca_property_parser minijson_reader.hpp sca_property_parser.cpp)
target_compile_features(minijson_example PRIVATE cxx_std_17)
target_link_libraries(sca_property_parser minijson::minijson_writer)

add_executable(${PROJECT_NAME}_tests minijson_reader.hpp minijson_reader_tests.cpp)
target_link_libraries(${PROJECT_NAME}_tests GTest::gtest_main)

foreach(prog minijson_example sca_property_parser ${PROJECT_NAME}_tests)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_SIMULATE_ID MATCHES "MSVC")
    target_compile_options(${prog} PRIVATE /W4 /WX /wd4244 /wd4996)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${prog} PRIVATE -Wall -Wextra -Werror -Wshadow -Wno-unknown-pragmas)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL 10.0)
      target_compile_options(${prog} PRIVATE -Wdeprecated-copy-dtor -Wnewline-eof)
    endif()
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(${prog} PRIVATE /W4 /WX /wd4244 /wd4996)
  endif()
  target_link_libraries(${prog} minijson::${PROJECT_NAME})
endforeach()

# ---- code coverage ----

if(NOT MSVC)
  option(ENABLE_TEST_COVERAGE "Enable test coverage" ON)
  if(ENABLE_TEST_COVERAGE AND "${CMAKE_BUILD_TYPE}" STREQUAL Debug)
    foreach(prog minijson_example sca_property_parser ${PROJECT_NAME}_tests)
      target_compile_options(${prog} PUBLIC -O0 -g -fprofile-arcs -ftest-coverage)
      target_link_options(${prog} PUBLIC -fprofile-arcs -ftest-coverage)
    endforeach()
  endif()
endif()

# ---- ctest ----

enable_testing()

add_test(NAME all_tests COMMAND ${PROJECT_NAME}_tests)
add_test(NAME minijson_example COMMAND minijson_example)

if(MSVC)
  add_test(NAME sca_simpleproperty_property COMMAND sca_property_parser simpleproperty.json WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME sca_structproperty_property COMMAND sca_property_parser structproperty.json WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME sca_structsequence_property COMMAND sca_property_parser structsequence.json WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME simpleBoolSequence COMMAND sca_property_parser simpleBoolSequence.json WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME simpleDoubleSequence COMMAND sca_property_parser simpleDoubleSequence.json WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME simpleLongSequence COMMAND sca_property_parser simpleLongSequence.json WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME simpleStringSequence COMMAND sca_property_parser simpleStringSequence.json WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
else()
  add_test(NAME sca_property_parser COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR} test)
endif()

# ---- install ----

install(FILES minijson_reader.hpp sca_property_parser DESTINATION include)

