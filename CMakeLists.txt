# You don't need to compile any library to use minijson_reader in your project:
# just include the minijson_reader.hpp header anywhere you need, and you're ready to go.

# Use this cmake project to build and run the unit tests (you will need Google Test):
#   $ mkdir build
#   $ cd build
#   $ cmake ..
#   $ make
#   $ ./minijson_reader_tests
# You can append -DCPP11:int=1 to the cmake command to build in C++11 mode.

cmake_minimum_required(VERSION 2.8.7)
project(minijson_reader CXX)

option(CPP11 "Build with c++11 standard enabled" off)
option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)." off)
option(CMAKE_CXX_EXTENSIONS "Default value for CXX_EXTENSIONS property of targets." off)
if (CPP11)
    set(CMAKE_CXX_STANDARD_REQUIRED 11 CACHE STRING "the requiered c++ standard")
else()
    set(CMAKE_CXX_STANDARD_REQUIRED 03 CACHE STRING "the requiered c++ standard")
endif()

if(MSVC)
    #FIXME Force to always compile with W4 or Wall
    #TODO Treats all compiler warnings as errors: WX
    #NO!: Disable Language Extensions: Za
    # see https://msdn.microsoft.com/en-us/library/19z1t1wy(v=vs.120).aspx
    # and https://msdn.microsoft.com/en-us/library/0k0w269d(v=vs.120).aspx
    if(CMAKE_CXX_FLAGS MATCHES ".* /W([1234]|all) .*")
        string(REGEX REPLACE "/W([1234]|all)" "/W3" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "/W4 /WX /std:c++lastest" CACHE STRING "MSVC compliler flags used" FORCE)
    endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    # Update if necessary
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Werror -Wno-unknown-pragmas")

    if(CMAKE_CXX_STANDARD_REQUIRED MATCHES "14")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    elseif(CMAKE_CXX_STANDARD_REQUIRED MATCHES "11")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++03")
        find_package(Boost 1.55 REQUIRED)
        include_directories(${Boost_INCLUDE_DIRS})
    endif()
endif()

# AppleClang
if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(CMAKE_CXX_FLAGS "-std=c++${CMAKE_CXX_STANDARD_REQUIRED} -Wall -Wextra -Werror -Wno-unknown-pragmas")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedef" CACHE STRING "clang compliler flags used" FORCE)
endif()

set(CMAKE_CXX_COMPILER_INFO "${CMAKE_CXX_COMPILER_ID} V${CMAKE_CXX_COMPILER_VERSION}" CACHE STRING "info")
message("${CMAKE_CXX_COMPILER_INFO} with -std=c++${CMAKE_CXX_STANDARD_REQUIRED} used")
message("CMAKE_CXX_FLAGS:=${CMAKE_CXX_FLAGS}")
message("CMAKE_CXX11_COMPILE_FEATURES:=${CMAKE_CXX11_COMPILE_FEATURES}")
message("CMAKE_CXX14_COMPILE_FEATURES:=${CMAKE_CXX14_COMPILE_FEATURES}")

# Where to put all the RUNTIME targets when built.  This variable is used to
# initialize the RUNTIME_OUTPUT_DIRECTORY property on all the targets.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)


set (GTest_DIR ../googletest CACHE PATH "gtest source dir")
set (GTEST_INCLUDE_DIRS ${GTest_DIR}/gtest/googletest/include)
set (GTEST_BOTH_LIBRARIES gtest gtest_main)
#TODO find_package(GTest REQUIRED HINTS /usr/local )

###########################################
# set(GTEST_INCLUDE_DIRS gtest-1.7.0/include)
# set(GTEST_BOTH_LIBRARIES gtest gtest_main)
include_directories(${GTEST_INCLUDE_DIRS})

add_subdirectory(${GTest_DIR} gtest)
###########################################

add_executable(minijson_example
    minijson_reader.hpp
    minijson_example.cpp
)
# see https://cmake.org/cmake/help/v3.7/prop_tgt/CXX_STANDARD.html#prop_tgt:CXX_STANDARD
# needs: cxx_lambdas, optional: cxx_final cxx_generalized_initializers
set_property(TARGET minijson_example PROPERTY CXX_STANDARD 11)
# https://cmake.org/cmake/help/v3.7/manual/cmake-compile-features.7.html#manual:cmake-compile-features(7)
target_compile_features(minijson_example PRIVATE cxx_lambdas)

add_executable(minijson_parser
    minijson_reader.hpp
    minijson_parser.cpp
)


enable_testing()

add_executable(minijson_reader_tests
    minijson_reader.hpp
    minijson_reader_tests.cpp
)

target_link_libraries(minijson_reader_tests ${GTEST_BOTH_LIBRARIES})

if(UNIX)
    target_link_libraries(minijson_reader_tests pthread)
endif()

add_test(NAME all COMMAND minijson_reader_tests)
