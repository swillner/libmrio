if(YAML_CPP_INCLUDE_DIR AND YAML_CPP_LIBRARY)
  set(YAML_CPP_FIND_QUIETLY TRUE)
endif()

find_path(YAML_CPP_INCLUDE_DIR NAMES yaml-cpp/yaml.h)
find_library(YAML_CPP_LIBRARY NAMES yaml-cpp)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(YAML_CPP DEFAULT_MSG YAML_CPP_INCLUDE_DIR YAML_CPP_LIBRARY)

mark_as_advanced(YAML_CPP_INCLUDE_DIR YAML_CPP_LIBRARY)

set(YAML_CPP_LIBRARIES ${YAML_CPP_LIBRARY} )
set(YAML_CPP_INCLUDE_DIRS ${YAML_CPP_INCLUDE_DIR} )

if(YAML_CPP_FOUND AND NOT TARGET yaml-cpp)
  add_library(yaml-cpp UNKNOWN IMPORTED)
  set_target_properties(yaml-cpp PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${YAML_CPP_INCLUDE_DIRS}")
  set_target_properties(yaml-cpp PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "CXX")
  set_target_properties(yaml-cpp PROPERTIES IMPORTED_LOCATION "${YAML_CPP_LIBRARIES}")
endif()