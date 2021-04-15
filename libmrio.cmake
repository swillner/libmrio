if(TARGET libmrio)
  return()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake)

add_library(libmrio STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/disaggregation.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/MRIOIndexSet.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/MRIOTable.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/ProxyData.cpp)
target_include_directories(libmrio PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/lib/cpp-library)
target_compile_options(libmrio PRIVATE "-std=c++11")

option(LIBMRIO_PARALLELIZATION "" ON)
if(LIBMRIO_PARALLELIZATION)
  find_package(OpenMP REQUIRED)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

option(LIBMRIO_WITH_NETCDF "NetCDF" ON)
if(LIBMRIO_WITH_NETCDF)
  find_package(NETCDF REQUIRED)
  message(STATUS "NetCDF include directory: ${NETCDF_INCLUDE_DIR}")
  message(STATUS "NetCDF library: ${NETCDF_LIBRARY}")
  target_link_libraries(libmrio PRIVATE netcdf)

include_netcdf_cxx4(libmrio ON v4.3.0)
  target_compile_definitions(libmrio PUBLIC LIBMRIO_WITH_NETCDF)
endif()

option(LIBMRIO_VERBOSE "Verbose debug output" OFF)
if(LIBMRIO_VERBOSE)
  target_compile_definitions(libmrio PUBLIC LIBMRIO_VERBOSE)
endif()

option(LIBMRIO_SHOW_PROGRESS "Show progress bars" ON)
if(LIBMRIO_SHOW_PROGRESS)
  target_compile_definitions(libmrio PUBLIC LIBMRIO_SHOW_PROGRESS)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.8)
    set_property(TARGET libmrio PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
  target_compile_definitions(libmrio PUBLIC NDEBUG)
else()
  target_compile_definitions(libmrio PRIVATE DEBUG)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/lib/settingsnode/settingsnode.cmake)
include_settingsnode(libmrio)
