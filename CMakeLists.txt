cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
include(cmake/helpers.cmake)
include(cmake/libraries.cmake)

add_system_paths()
set_default_build_type(Release)

project(mrio_disaggregate)

add_executable(mrio_disaggregate src/main.cpp)

target_include_directories(mrio_disaggregate PRIVATE include)
target_compile_options(mrio_disaggregate PRIVATE "-std=c++11")

set_advanced_cpp_warnings(mrio_disaggregate)
set_build_type_specifics(mrio_disaggregate)
add_git_version(mrio_disaggregate
  FALLBACK_VERSION 1.0.0)

include(libmrio.cmake)
set_advanced_cpp_warnings(libmrio)
target_link_libraries(mrio_disaggregate PRIVATE libmrio)

include(lib/settingsnode/settingsnode.cmake)
include_settingsnode(mrio_disaggregate)

include_yaml_cpp(mrio_disaggregate ON "yaml-cpp-0.6.2")

add_cpp_tools(mrio_disaggregate)
add_cpp_tools(libmrio)

include(CTest)

if(BUILD_TESTING)
  file(GLOB tests "${CMAKE_SOURCE_DIR}/tests/*")
  foreach(test ${tests})
    if(IS_DIRECTORY ${test})
      add_test(NAME "${test}"
        COMMAND sh ${CMAKE_SOURCE_DIR}/tests/run_test.sh $<TARGET_FILE:mrio_disaggregate>
        WORKING_DIRECTORY ${test})
    endif()
  endforeach()
endif()
