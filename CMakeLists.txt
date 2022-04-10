# You don't need to compile any library to use minijson_reader in your project:
# just include the minijson_reader.hpp header anywhere you need, and you're ready to go.

# Use this cmake project to build and run the unit tests (you will need Google Test):
#   $ mkdir build
#   $ cd build
#   $ cmake ..
#   $ make
#   $ ./minijson_reader_tests

cmake_minimum_required(VERSION 3.18.4)

project(minijson_reader)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Force to always compile with W4
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")
endif()

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

enable_testing()

add_executable(minijson_reader_tests
    minijson_reader_tests.cpp
)

target_link_libraries(minijson_reader_tests
    ${GTEST_BOTH_LIBRARIES})

if(UNIX)
    target_link_libraries(minijson_reader_tests
        pthread)
endif()

add_test(NAME all
    COMMAND minijson_reader_tests)
