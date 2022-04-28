# Copyright (c) Giacomo Drago <giacomo@giacomodrago.com>
# All rights reserved.

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of Giacomo Drago nor the
#    names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.

# THIS SOFTWARE IS PROVIDED BY GIACOMO DRAGO "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL GIACOMO DRAGO BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${GTEST_INCLUDE_DIRS})

enable_testing()

add_executable(test_main test/main.cpp)
target_link_libraries(test_main ${GTEST_BOTH_LIBRARIES})
add_test(NAME test_main COMMAND test_main)

add_executable(test_value_as test/value_as.cpp)
target_link_libraries(test_value_as ${GTEST_BOTH_LIBRARIES})
add_test(NAME test_value_as COMMAND test_value_as)

add_executable(test_dispatcher test/dispatcher.cpp)
target_link_libraries(test_dispatcher ${GTEST_BOTH_LIBRARIES})
add_test(NAME test_dispatcher COMMAND test_dispatcher)

if(UNIX)
    target_link_libraries(test_main pthread)
    target_link_libraries(test_value_as pthread)
    target_link_libraries(test_dispatcher pthread)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    include(cmake_modules/CodeCoverage.cmake)
    append_coverage_compiler_flags()
    set(GCOVR_ADDITIONAL_ARGS
        --fail-under-line 100
        --exclude-unreachable-branches
        --exclude-throw-branches
        --print-summary
    )

    setup_target_for_coverage_gcovr_html(
        NAME coverage
        DEPENDENCIES test_main test_value_as test_dispatcher
        EXECUTABLE ctest
        EXCLUDE "test/main.cpp" "test/value_as.cpp" "test/dispatcher.cpp"
    )
endif()
