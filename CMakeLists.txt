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

option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)." off)

if(MSVC)
    # Force to always compile with W4
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    # Update if necessary
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-unknown-pragmas")

    if(CMAKE_CXX14_COMPILE_FEATURES)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    elseif(CPP11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++03")
        find_package(Boost 1.55 REQUIRED)
        include_directories(${Boost_INCLUDE_DIRS})
    endif()
endif()

# AppleClang
if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -Werror -Wno-unknown-pragmas")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedef" CACHE STRING "clang compliler flags used" FORCE)
endif()

set(CMAKE_CXX_COMPILER_INFO "${CMAKE_CXX_COMPILER_ID} V${CMAKE_CXX_COMPILER_VERSION}" CACHE STRING "info")
message("${CMAKE_CXX_COMPILER_INFO} used")

#TODO set (GTEST_INCLUDE_DIRS /usr/local/include)
#TODO set (GTEST_BOTH_LIBRARIES /usr/local/lib/libgtest.la /usr/local/lib/libgtest_main.la)
#FIXME find_package(GTest REQUIRED HINTS /usr/local ../ninja/gtest-1.7.0)

###########################################
set(GTEST_INCLUDE_DIRS gtest-1.7.0/include)
set(GTEST_BOTH_LIBRARIES gtest gtest_main)
include_directories(${GTEST_INCLUDE_DIRS})

add_subdirectory(gtest-1.7.0)
###########################################

add_executable(minijson_example
    minijson_reader.hpp
    minijson_example.cpp
)

enable_testing()

add_executable(minijson_reader_tests
    minijson_reader.hpp
    minijson_reader_tests.cpp
)

target_link_libraries(minijson_reader_tests
    ${GTEST_BOTH_LIBRARIES})

if(UNIX)
    target_link_libraries(minijson_reader_tests
        pthread)
endif()

add_test(NAME all COMMAND minijson_reader_tests)
