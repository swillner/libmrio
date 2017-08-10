cmake_minimum_required(VERSION 2.8)
if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.8)
  cmake_policy(SET CMP0069 NEW) # for INTERPROCEDURAL_OPTIMIZATION
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

project(mrio_disaggregate)

set(MRIO_DISAGGREGATE_VERSION 2.1.0)
if(EXISTS "${CMAKE_SOURCE_DIR}/.git" AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
  find_program(HAVE_GIT git)
  if(HAVE_GIT)
    execute_process(COMMAND git describe --tags --dirty --always OUTPUT_VARIABLE GIT_OUTPUT OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX REPLACE "^v([0-9]+\\.[0-9]+)\\.(0-)?([0-9]*)((-.+)?)$" "\\1.\\3\\4" MRIO_DISAGGREGATE_VERSION ${GIT_OUTPUT})
  endif()
  mark_as_advanced(HAVE_GIT)
endif()
message(STATUS "Program version: ${MRIO_DISAGGREGATE_VERSION}")

add_executable(mrio_disaggregate src/main.cpp)

target_include_directories(mrio_disaggregate PRIVATE include)
target_compile_options(mrio_disaggregate PRIVATE "-std=c++11")
target_compile_definitions(mrio_disaggregate PRIVATE MRIO_DISAGGREGATE_VERSION="${MRIO_DISAGGREGATE_VERSION}")

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.8)
    message(STATUS "Enabling interprocedural optimization")
    set_property(TARGET mrio_disaggregate PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
  target_compile_definitions(mrio_disaggregate PUBLIC NDEBUG)
else()
  target_compile_definitions(mrio_disaggregate PRIVATE DEBUG)
endif()

include(libmrio.cmake)
target_link_libraries(mrio_disaggregate libmrio)

include(lib/settingsnode/settingsnode.cmake)
target_link_libraries(mrio_disaggregate settingsnode)
