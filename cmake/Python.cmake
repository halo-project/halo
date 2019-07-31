# BSD 3-Clause License
#
# Copyright (c) 2018, Kavon Farvardin. All rights reserved.
#
# Copyright (c) 2018, Juan Manuel Martinez Caamaño and Serge Guelton and Quarkslab.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


include(FindPackageHandleStandardArgs)

# allow specifying which Python installation to use
if (NOT PYTHON_EXEC)
  set(PYTHON_EXEC $ENV{PYTHON_EXEC})
endif (NOT PYTHON_EXEC)

if (NOT PYTHON_EXEC)
  find_program(PYTHON_EXEC "python${Python_FIND_VERSION}"
               DOC "Location of python executable to use")
endif(NOT PYTHON_EXEC)

execute_process(COMMAND "${PYTHON_EXEC}" "-c"
"import sys; print('%d.%d' % (sys.version_info[0],sys.version_info[1]))"
OUTPUT_VARIABLE PYTHON_VERSION
OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE "." "" PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})


function(find_python_module module)
  string(TOUPPER ${module} module_upper)
  if(NOT PY_${module_upper})
    if(ARGC GREATER 1 AND ARGV1 STREQUAL "REQUIRED")
      set(${module}_FIND_REQUIRED TRUE)
    endif()
  # A module's location is usually a directory, but for binary modules it's a .so file.
    execute_process(COMMAND "${PYTHON_EXEC}" "-c" "import re, ${module}; print re.compile('/__init__.py.*').sub('',${module}.__file__)"
                    RESULT_VARIABLE _${module}_status
                    OUTPUT_VARIABLE _${module}_location
                    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _${module}_status)
      set(PY_${module_upper} ${_${module}_location} CACHE STRING "Location of Python module ${module}")
    endif(NOT _${module}_status)
  endif(NOT PY_${module_upper})
  find_package_handle_standard_args(PY_${module} DEFAULT_MSG PY_${module_upper})
endfunction(find_python_module)
