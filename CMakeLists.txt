cmake_minimum_required(VERSION 3.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
include(cmake/helpers.cmake)
include(cmake/libraries.cmake)

set_default_build_type(Release)

project(mrio_disaggregate)

add_executable(mrio_disaggregate src/main.cpp)

target_include_directories(mrio_disaggregate PRIVATE include)
set_property(TARGET mrio_disaggregate PROPERTY CXX_STANDARD 11)

set_advanced_cpp_warnings(mrio_disaggregate)
set_build_type_specifics(mrio_disaggregate)
add_git_version(mrio_disaggregate)

include(libmrio.cmake)
set_advanced_cpp_warnings(libmrio)
target_link_libraries(mrio_disaggregate PRIVATE libmrio)

include(lib/settingsnode/settingsnode.cmake)
include_settingsnode(mrio_disaggregate)

include_yaml_cpp(mrio_disaggregate ON "yaml-cpp-0.6.2")

add_cpp_tools(mrio_disaggregate)
add_cpp_tools(libmrio)
