set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake)

add_library(libmrio STATIC ${CMAKE_CURRENT_LIST_DIR}/src/Disaggregation.cpp ${CMAKE_CURRENT_LIST_DIR}/src/MRIOIndexSet.cpp ${CMAKE_CURRENT_LIST_DIR}/src/MRIOTable.cpp)
target_include_directories(libmrio PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/lib/cpp-library)

option(LIBMRIO_WITH_NETCDF "NetCDF" ON)
if(LIBMRIO_WITH_NETCDF)
  find_package(NETCDF REQUIRED)
  message(STATUS "NetCDF include directory: ${NETCDF_INCLUDE_DIR}")
  message(STATUS "NetCDF library: ${NETCDF_LIBRARY}")
  target_link_libraries(libmrio netcdf)

  find_package(NETCDF_CPP4 REQUIRED)
  message(STATUS "NetCDF_c++4 include directory: ${NETCDF_CPP4_INCLUDE_DIR}")
  message(STATUS "NetCDF_c++4 library: ${NETCDF_CPP4_LIBRARY}")
  target_link_libraries(libmrio netcdf_c++4)

  target_compile_definitions(libmrio PRIVATE LIBMRIO_WITH_NETCDF)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/lib/settingsnode/settingsnode.cmake)
target_link_libraries(libmrio settingsnode)
