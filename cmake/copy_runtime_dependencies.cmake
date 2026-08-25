if(NOT DEFINED IVAN_EXECUTABLE
   OR NOT DEFINED IVAN_DESTINATION
   OR NOT DEFINED IVAN_DEPENDENCY_ROOT)
  message(FATAL_ERROR "Runtime dependency copy arguments are incomplete")
endif()

if(POLICY CMP0207)
  cmake_policy(SET CMP0207 NEW)
endif()

file(REAL_PATH "${IVAN_DEPENDENCY_ROOT}" _dependency_root)
file(GET_RUNTIME_DEPENDENCIES
     EXECUTABLES "${IVAN_EXECUTABLE}"
     DIRECTORIES "${_dependency_root}/bin"
     RESOLVED_DEPENDENCIES_VAR _resolved_dependencies
     UNRESOLVED_DEPENDENCIES_VAR _unresolved_dependencies)

foreach(_dependency IN LISTS _resolved_dependencies)
  if(NOT _dependency MATCHES "\\.dll$")
    continue()
  endif()

  file(REAL_PATH "${_dependency}" _resolved_dependency)
  string(FIND "${_resolved_dependency}" "${_dependency_root}" _root_index)
  if(_root_index EQUAL 0)
    file(COPY "${_dependency}" DESTINATION "${IVAN_DESTINATION}")
    message(STATUS "Copied MinGW runtime dependency: ${_dependency}")
  endif()
endforeach()

if(_unresolved_dependencies)
  message(WARNING "Unresolved runtime dependencies: ${_unresolved_dependencies}")
endif()
