#[=======================================================================[

FindRaii
---------

Find raii includes and library.

Imported Targets
^^^^^^^^^^^^^^^^

An :ref:`imported target <Imported targets>` named
``RAII::RAII`` is provided if raii has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``RAII_FOUND``
  True if raii was found, false otherwise.
``RAII_INCLUDE_DIRS``
  Include directories needed to include raii headers.
``RAII_LIBRARIES``
  Libraries needed to link to raii.
``RAII_VERSION``
  The version of raii found.
``RAII_VERSION_MAJOR``
  The major version of raii.
``RAII_VERSION_MINOR``
  The minor version of raii.
``RAII_VERSION_PATCH``
  The patch version of raii.

Cache Variables
^^^^^^^^^^^^^^^

This module uses the following cache variables:

``RAII_LIBRARY``
  The location of the raii library file.
``RAII_INCLUDE_DIR``
  The location of the raii include directory containing ``raii.h``.

The cache variables should not be used by project code.
They may be set by end users to point at raii components.
#]=======================================================================]

find_library(raii_LIBRARY
  NAMES raii
  )
mark_as_advanced(raii_LIBRARY)

find_path(raii_INCLUDE_DIR
  NAMES raii.h
  )
mark_as_advanced(raii_INCLUDE_DIR)

#-----------------------------------------------------------------------------
# Extract version number if possible.
set(_RAII_H_REGEX "#[ \t]*define[ \t]+RAII_VERSION_(MAJOR|MINOR|PATCH)[ \t]+[0-9]+")
if(RAII_INCLUDE_DIR AND EXISTS "${RAII_INCLUDE_DIR}/raii.h")
  file(STRINGS "${RAII_INCLUDE_DIR}/raii.h" _RAII_H REGEX "${_RAII_H_REGEX}")
else()
  set(_RAII_H "")
endif()
foreach(c MAJOR MINOR PATCH)
  if(_RAII_H MATCHES "#[ \t]*define[ \t]+RAII_VERSION_${c}[ \t]+([0-9]+)")
    set(_RAII_VERSION_${c} "${CMAKE_MATCH_1}")
  else()
    unset(_RAII_VERSION_${c})
  endif()
endforeach()

if(DEFINED _RAII_VERSION_MAJOR AND DEFINED _RAII_VERSION_MINOR)
  set(RAII_VERSION_MAJOR "${_RAII_VERSION_MAJOR}")
  set(RAII_VERSION_MINOR "${_RAII_VERSION_MINOR}")
  set(RAII_VERSION "${RAII_VERSION_MAJOR}.${RAII_VERSION_MINOR}")
  if(DEFINED _RAII_VERSION_PATCH)
    set(RAII_VERSION_PATCH "${_RAII_VERSION_PATCH}")
    set(RAII_VERSION "${RAII_VERSION}.${RAII_VERSION_PATCH}")
  else()
    unset(RAII_VERSION_PATCH)
  endif()
else()
  set(RAII_VERSION_MAJOR "")
  set(RAII_VERSION_MINOR "")
  set(RAII_VERSION_PATCH "")
  set(RAII_VERSION "")
endif()
unset(_RAII_VERSION_MAJOR)
unset(_RAII_VERSION_MINOR)
unset(_RAII_VERSION_PATCH)
unset(_RAII_H_REGEX)
unset(_RAII_H)

#-----------------------------------------------------------------------------
# Set Find Package Arguments
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(raii
    FOUND_VAR raii_FOUND
    REQUIRED_VARS RAII_LIBRARY RAII_INCLUDE_DIR
    VERSION_VAR RAII_VERSION
    HANDLE_COMPONENTS
        FAIL_MESSAGE
        "Could NOT find Raii"
)

set(RAII_FOUND ${raii_FOUND})

#-----------------------------------------------------------------------------
# Provide documented result variables and targets.
if(RAII_FOUND)
  set(RAII_INCLUDE_DIRS ${RAII_INCLUDE_DIR})
  set(RAII_LIBRARIES ${RAII_LIBRARY})
  if(NOT TARGET RAII::RAII)
    add_library(RAII::RAII UNKNOWN IMPORTED)
    set_target_properties(RAII::RAII PROPERTIES
      IMPORTED_LOCATION "${RAII_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${RAII_INCLUDE_DIRS}"
      )
  endif()
endif()
