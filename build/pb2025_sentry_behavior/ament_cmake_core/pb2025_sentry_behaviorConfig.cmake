# generated from ament/cmake/core/templates/nameConfig.cmake.in

# prevent multiple inclusion
if(_pb2025_sentry_behavior_CONFIG_INCLUDED)
  # ensure to keep the found flag the same
  if(NOT DEFINED pb2025_sentry_behavior_FOUND)
    # explicitly set it to FALSE, otherwise CMake will set it to TRUE
    set(pb2025_sentry_behavior_FOUND FALSE)
  elseif(NOT pb2025_sentry_behavior_FOUND)
    # use separate condition to avoid uninitialized variable warning
    set(pb2025_sentry_behavior_FOUND FALSE)
  endif()
  return()
endif()
set(_pb2025_sentry_behavior_CONFIG_INCLUDED TRUE)

# output package information
if(NOT pb2025_sentry_behavior_FIND_QUIETLY)
  message(STATUS "Found pb2025_sentry_behavior: 0.0.0 (${pb2025_sentry_behavior_DIR})")
endif()

# warn when using a deprecated package
if(NOT "" STREQUAL "")
  set(_msg "Package 'pb2025_sentry_behavior' is deprecated")
  # append custom deprecation text if available
  if(NOT "" STREQUAL "TRUE")
    set(_msg "${_msg} ()")
  endif()
  # optionally quiet the deprecation message
  if(NOT ${pb2025_sentry_behavior_DEPRECATED_QUIET})
    message(DEPRECATION "${_msg}")
  endif()
endif()

# flag package as ament-based to distinguish it after being find_package()-ed
set(pb2025_sentry_behavior_FOUND_AMENT_PACKAGE TRUE)

# include all config extra files
set(_extras "ament_cmake_export_dependencies-extras.cmake;ament_cmake_export_include_directories-extras.cmake;ament_cmake_export_libraries-extras.cmake")
foreach(_extra ${_extras})
  include("${pb2025_sentry_behavior_DIR}/${_extra}")
endforeach()
