#  Copyright (C) 2017 Sven Willner <sven.willner@gmail.com>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as published
#  by the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

function(get_depends_properties RESULT_NAME TARGET PROPERTIES)
  foreach(PROPERTY ${PROPERTIES})
    set(RESULT_${PROPERTY})
  endforeach()
  get_target_property(INTERFACE_LINK_LIBRARIES ${TARGET} INTERFACE_LINK_LIBRARIES)
  if(INTERFACE_LINK_LIBRARIES)
    foreach(INTERFACE_LINK_LIBRARY ${INTERFACE_LINK_LIBRARIES})
      if(TARGET ${INTERFACE_LINK_LIBRARY})
        get_depends_properties(TMP ${INTERFACE_LINK_LIBRARY} "${PROPERTIES}")
        foreach(PROPERTY ${PROPERTIES})
          set(RESULT_${PROPERTY} ${RESULT_${PROPERTY}} ${TMP_${PROPERTY}})
        endforeach()
      endif()
    endforeach()
  endif()
  foreach(PROPERTY ${PROPERTIES})
    get_target_property(TMP ${TARGET} ${PROPERTY})
    if(TMP)
      set(RESULT_${PROPERTY} ${RESULT_${PROPERTY}} ${TMP})
    endif()
    set(${RESULT_NAME}_${PROPERTY} ${RESULT_${PROPERTY}} PARENT_SCOPE)
  endforeach()
endfunction()

function(get_all_include_directories RESULT_NAME TARGET)
  get_depends_properties(RESULT ${TARGET} "INTERFACE_INCLUDE_DIRECTORIES;INTERFACE_SYSTEM_INCLUDE_DIRECTORIES")
  set(RESULT ${RESULT_INTERFACE_INCLUDE_DIRECTORIES} ${RESULT_INTERFACE_SYSTEM_INCLUDE_DIRECTORIES})
  get_target_property(INCLUDE_DIRECTORIES ${TARGET} INCLUDE_DIRECTORIES)
  if(INCLUDE_DIRECTORIES)
    set(RESULT ${RESULT} ${INCLUDE_DIRECTORIES})
  endif()
  if(RESULT)
    list(REMOVE_DUPLICATES RESULT)
  endif()
  set(${RESULT_NAME} ${RESULT} PARENT_SCOPE)
endfunction()

function(get_all_compile_definitions RESULT_NAME TARGET)
  get_depends_properties(RESULT ${TARGET} "INTERFACE_COMPILE_DEFINITIONS")
  set(RESULT ${RESULT_INTERFACE_COMPILE_DEFINITIONS})
  get_target_property(COMPILE_DEFINITIONS ${TARGET} COMPILE_DEFINITIONS)
  if(COMPILE_DEFINITIONS)
    set(RESULT ${RESULT} ${COMPILE_DEFINITIONS})
  endif()
  if(RESULT)
    list(REMOVE_DUPLICATES RESULT)
  endif()
  set(${RESULT_NAME} ${RESULT} PARENT_SCOPE)
endfunction()

function(add_on_source TARGET COMMAND)
  find_program(${COMMAND}_PATH ${COMMAND})
  if(${COMMAND}_PATH)

    list(REMOVE_AT ARGV 0) # remove TARGET
    list(REMOVE_AT ARGV 0) # remove COMMAND

    set(ARGS)
    set(PER_SOURCEFILE FALSE)

    foreach(ARG ${ARGV})
      if(${ARG} STREQUAL "INCLUDES")
        get_all_include_directories(INCLUDE_DIRECTORIES ${TARGET})
        if(INCLUDE_DIRECTORIES)
          foreach(INCLUDE_DIRECTORY ${INCLUDE_DIRECTORIES})
            set(ARGS ${ARGS} "-I${INCLUDE_DIRECTORY}")
          endforeach()
        endif()
      elseif(${ARG} STREQUAL "DEFINITIONS")
        get_all_compile_definitions(COMPILE_DEFINITIONS ${TARGET})
        if(COMPILE_DEFINITIONS)
          foreach(COMPILE_DEFINITION ${COMPILE_DEFINITIONS})
            set(ARGS ${ARGS} "-D${COMPILE_DEFINITION}")
          endforeach()
        endif()
      elseif(${ARG} STREQUAL "ALL_SOURCEFILES")
        get_target_property(SOURCES ${TARGET} SOURCES)
        set(ARGS ${ARGS} ${SOURCES})
      elseif(${ARG} STREQUAL "SOURCEFILE")
        set(ARGS ${ARGS} ${ARG})
        set(PER_SOURCEFILE TRUE)
      else()
        set(ARGS ${ARGS} ${ARG})
      endif()
    endforeach()

    if(PER_SOURCEFILE)
      get_target_property(SOURCES ${TARGET} SOURCES)
      set(COMMANDS)
      foreach(FILE ${SOURCES})
        set(LOCAL_ARGS)
        foreach(ARG ${ARGS})
          if(${ARG} STREQUAL "SOURCEFILE")
            set(LOCAL_ARGS ${LOCAL_ARGS} ${FILE})
          else()
            set(LOCAL_ARGS ${LOCAL_ARGS} ${ARG})
          endif()
        endforeach()
        file(GLOB FILE ${FILE})
        file(RELATIVE_PATH FILE ${CMAKE_CURRENT_SOURCE_DIR} ${FILE})
        add_custom_command(
          OUTPUT ${TARGET}_${COMMAND}/${FILE}
          COMMAND ${${COMMAND}_PATH} ${LOCAL_ARGS}
          WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
          COMMENT "Running ${COMMAND} on ${FILE}..."
          VERBATIM)
        set_source_files_properties(${TARGET}_${COMMAND}/${FILE} PROPERTIES SYMBOLIC TRUE)
        set(COMMANDS ${COMMANDS} ${TARGET}_${COMMAND}/${FILE})
      endforeach()
      add_custom_target(
        ${TARGET}_${COMMAND}
        DEPENDS ${COMMANDS}
        COMMENT "Running ${COMMAND} on ${TARGET}...")
    else()
      add_custom_target(
        ${TARGET}_${COMMAND}
        COMMAND ${${COMMAND}_PATH} ${ARGS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running ${COMMAND} on ${TARGET}..."
        VERBATIM)
    endif()
  endif()
endfunction()

function(add_cpp_tools TARGET)
  add_on_source(${TARGET} clang-format -i --style=file ALL_SOURCEFILES)
  add_on_source(${TARGET} cppcheck INCLUDES DEFINITIONS --quiet --template=gcc --enable=all ALL_SOURCEFILES)
  add_on_source(${TARGET} cppclean INCLUDES ALL_SOURCEFILES)
  add_on_source(${TARGET} iwyu -std=c++11 -I/usr/include/clang/3.8/include INCLUDES DEFINITIONS SOURCEFILE)
endfunction()
