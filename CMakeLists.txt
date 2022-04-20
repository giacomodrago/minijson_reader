# You don't need to compile any library to use minijson_reader in your project:
# just include the minijson_reader.hpp header anywhere you need, and you're ready to go.

# Use this cmake project to build and run the unit tests (Google Test required)
# and create a coverage report (gcc only):
#   $ mkdir build
#   $ cd build
#   $ cmake .. -DCMAKE_BUILD_TYPE=Debug
#   $ make
#   $ ctest
#   $ make coverage
#   $ firefox coverage/index.html

cmake_minimum_required(VERSION 3.18.4)

project(minijson_reader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Force to always compile with /W4 /WX
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4 /WX" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
    endif()
elseif(CMAKE_COMPILER_IS_GNUCXX)
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

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    include(cmake_modules/CodeCoverage.cmake)
    append_coverage_compiler_flags()
    set(GCOVR_ADDITIONAL_ARGS
        --fail-under-line 100
        --exclude-unreachable-branches
        --exclude-throw-branches
        --print-summary)

    setup_target_for_coverage_gcovr_html(
        NAME coverage
        DEPENDENCIES minijson_reader_tests
        EXECUTABLE minijson_reader_tests
        EXCLUDE "minijson_reader_tests.cpp")
endif()
