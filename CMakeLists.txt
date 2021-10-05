# You don't need to compile any library to use minijson_reader in your project:
# just include the minijson_reader.hpp header anywhere you need, and you're ready to go.

# Use this cmake project to build and run the unit tests (you will need Google Test):
#   $ mkdir build
#   $ cd build
#   $ cmake ..
#   $ make
#   $ ctest
#   $ make install

cmake_minimum_required(VERSION 3.14...3.21)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(
  minijson_reader
  VERSION 0.9.0
  LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(MINIJSON_MASTER_PROJECT YES)
endif()

option(MINIJSON_INSTALL "Generate the install target" ${MINIJSON_MASTER_PROJECT})
option(CMAKE_CXX_EXTENSIONS "Default value for CXX_EXTENSIONS property of targets." NO)
option(CMAKE_CXX_STANDARD_REQUIRED "The c++ standard is requrired" YES)
set(CMAKE_CXX_STANDARD
    17
    CACHE STRING "The used c++ standard"
)

# Where to put all the RUNTIME targets when build. (ddl, and exe)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---- dependency ----
include(cmake/CPM.cmake)

option(BUILD_TESTS "Build tests" ${MINIJSON_MASTER_PROJECT})
if(BUILD_TESTS)
  CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG release-1.11.0
    VERSION 1.11.0
    OPTIONS "BUILD_GMOCK OFF" "gtest_force_shared_crt"
  )
endif()

CPMAddPackage(
  NAME minijson_writer
  GITHUB_REPOSITORY ClausKlein/minijson_writer
  GIT_TAG 49021439cdb3b6c1029f0aa33a9522855b1f14ee # v0.9.0
  OPTIONS "BUILD_TESTS ${BUILD_TESTS}" "MINIJSON_INSTALL ${MINIJSON_INSTALL}"
)

CPMAddPackage(
  NAME magic_enum
  GITHUB_REPOSITORY ClausKlein/magic_enum
  GIT_TAG 35952ac46a144e67d58613b4335345027cb2d5ce
)

CPMAddPackage(
  NAME pfr
  GITHUB_REPOSITORY boostorg/pfr
  GIT_TAG boost-1.77.0
)

# ---- Create library ----

# Note: for header-only libraries change all PUBLIC flags to INTERFACE
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(
  ${PROJECT_NAME} BEFORE INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}> $<INSTALL_INTERFACE:include>
)
# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(${PROJECT_NAME} INTERFACE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

add_library(minijson::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# ---- Create executables ----

if(BUILD_TESTS)
  add_executable(minijson_example minijson_reader.hpp minijson_example.cpp)
  target_compile_features(minijson_example PRIVATE cxx_lambdas)

  add_executable(sca_property_parser minijson_reader.hpp sca_property_parser.cpp)
  target_compile_features(minijson_example PRIVATE cxx_std_17)
  target_link_libraries(sca_property_parser minijson::minijson_writer magic_enum::magic_enum Boost::pfr)

  add_executable(test_${PROJECT_NAME} minijson_reader.hpp test/minijson_reader_tests.cpp)
  target_link_libraries(test_${PROJECT_NAME} GTest::gtest_main)

  foreach(prog minijson_example sca_property_parser test_${PROJECT_NAME})
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
    option(ENABLE_TEST_COVERAGE "Enable test coverage" YES)
    if(ENABLE_TEST_COVERAGE AND "${CMAKE_BUILD_TYPE}" STREQUAL Debug)
      foreach(prog minijson_example sca_property_parser test_${PROJECT_NAME})
        target_compile_options(${prog} PUBLIC -O0 -g -fprofile-arcs -ftest-coverage)
        target_link_options(${prog} PUBLIC -fprofile-arcs -ftest-coverage)
      endforeach()
    endif()
  endif()

  # ---- ctest ----

  enable_testing()

  add_test(NAME test_${PROJECT_NAME} COMMAND test_${PROJECT_NAME})
  add_test(NAME minijson_example COMMAND minijson_example)

  add_test(
    NAME sca_simpleproperty_property
    COMMAND sca_property_parser simpleproperty.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(
    NAME sca_structproperty_property
    COMMAND sca_property_parser structproperty.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(
    NAME sca_structsequence_property
    COMMAND sca_property_parser structsequence.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(
    NAME simpleBoolSequence
    COMMAND sca_property_parser simpleBoolSequence.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(
    NAME simpleDoubleSequence
    COMMAND sca_property_parser simpleDoubleSequence.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(
    NAME simpleLongSequence
    COMMAND sca_property_parser simpleLongSequence.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(
    NAME simpleStringSequence
    COMMAND sca_property_parser simpleStringSequence.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

endif()

# ---- examples using boost json, describe, mp11 ----

#
# see https://www.boost.org/users/download/
#
CPMAddPackage(
  NAME Boost
  VERSION 1.77.0
  URL https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.tar.bz2
  URL_HASH SHA256=fc9f85fc030e233142908241af7a846e60630aa7388de9a5fafb1f3a26840854
  DOWNLOAD_ONLY True
)
if(Boost_ADDED)
  # Define the header-only Boost target
  add_library(Boost::headers INTERFACE IMPORTED GLOBAL)
  target_include_directories(Boost::headers SYSTEM INTERFACE ${Boost_SOURCE_DIR})

  # Disable autolink
  target_compile_definitions(
    Boost::headers
    INTERFACE $<$<CONFIG:Release>:BOOST_DISABLE_ASSERT>
              BOOST_ASIO_NO_DEPRECATED
              BOOST_SYSTEM_NO_DEPRECATED
              BOOST_THREAD_VERSION=5
              BOOST_THREAD_USES_CHRONO
              BOOST_THREAD_PROVIDES_EXECUTORS
              BOOST_ALL_NO_LIB
  )

  CPMAddPackage(
    NAME json
    GITHUB_REPOSITORY boostorg/json
    GIT_TAG boost-1.77.0
    OPTIONS "BOOST_JSON_STANDALONE" "BOOST_JSON_INSTALL"
  )

  # CPMAddPackage(
  #   NAME describe
  #   GITHUB_REPOSITORY boostorg/regex
  #   GIT_TAG boost-1.77.0
  #   OPTIONS "BOOST_REGEX_STANDALONE"
  # )

  CPMAddPackage(
    NAME program_options
    GITHUB_REPOSITORY ClausKlein/program_options
    GIT_TAG af41cd4b6e2ae7fa8d947bb6a8dae8c98884bad5 # boost-1.77.0
  )

  # CPMAddPackage(
  #   NAME describe
  #   GITHUB_REPOSITORY boostorg/describe
  #   GIT_TAG boost-1.77.0
  # )

  # CPMAddPackage(
  #   NAME mp11
  #   GITHUB_REPOSITORY boostorg/mp11
  #   GIT_TAG boost-1.77.0
  # )

  add_executable(grep grep.cpp)
  target_link_libraries(grep Boost::program_options)

  add_executable(json_rpc json_rpc.cpp)
  target_link_libraries(json_rpc Boost::headers Boost::json)

  add_executable(value_to value_to.cpp)
  target_link_libraries(value_to Boost::headers Boost::json)

  add_executable(value_from value_from.cpp)
  target_link_libraries(value_from Boost::headers Boost::json)
endif()

# ---- install ----

if(MINIJSON_INSTALL)
  # TODO: export cmake config packages too! CK
  install(FILES minijson_reader.hpp DESTINATION include)
endif()
