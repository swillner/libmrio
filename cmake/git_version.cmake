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

execute_process(
  COMMAND git describe --tags --dirty --always
  WORKING_DIRECTORY ${ARGS_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_OUTPUT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX REPLACE "^v([0-9]+\\.[0-9]+)\\.(0-)?([0-9]*)((-.+)?)$" "\\1.\\3\\4" GIT_VERSION "${GIT_OUTPUT}")


execute_process(
  COMMAND git diff HEAD --no-color
  WORKING_DIRECTORY ${ARGS_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_DIFF
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
if(GIT_DIFF)
  string(MD5 GIT_DIFF_HASH "${GIT_DIFF}")
  string(SUBSTRING "${GIT_DIFF_HASH}" 0 12 GIT_DIFF_HASH)
  set(HAS_DIFF "true")
  set(GIT_DIFF "R\"${GIT_DIFF_HASH}(${GIT_DIFF})${GIT_DIFF_HASH}\"")
else()
  set(HAS_DIFF "false")
  set(GIT_DIFF "\"\"")
endif()

file(WRITE ${ARGS_BINARY_DIR}/include/version.h.new "\
#ifndef ${ARGS_DPREFIX}_VERSION_H
#define ${ARGS_DPREFIX}_VERSION_H

namespace ${ARGS_NAMESPACE} {

extern const char* version;
extern const char* git_diff;
constexpr bool has_diff = ${HAS_DIFF};

}  // namespace ${ARGS_NAMESPACE}

#endif
"
  )
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ARGS_BINARY_DIR}/include/version.h.new
          ${ARGS_BINARY_DIR}/include/version.h
)

file(WRITE ${ARGS_BINARY_DIR}/src/version.cpp.new "\
namespace ${ARGS_NAMESPACE} {

const char* version = \"${GIT_VERSION}\";
const char* git_diff = ${GIT_DIFF};

}  // namespace ${ARGS_NAMESPACE}
"
  )
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ARGS_BINARY_DIR}/src/version.cpp.new
          ${ARGS_BINARY_DIR}/src/version.cpp
)
