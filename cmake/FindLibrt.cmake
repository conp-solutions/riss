# - Check for the presence of RT
# 
# The following variables are set when RT is found:
#  LIBRT_FOUND      = Set to true, if all components of RT
#                     have been found.
#  LIBRT_INCLUDES   = Include path for the header files of RT
#  LIBRT_LIBRARIES  = Link these to use RT

## -----------------------------------------------------------------------------
## Check for the header files

find_path(LIBRT_INCLUDES time.h
          PATHS /usr/local/include /usr/include ${CMAKE_EXTRA_INCLUDES})

## -----------------------------------------------------------------------------
## Check for the library

find_library(LIBRT_LIBRARIES rt
             PATHS /usr/local/lib /usr/lib /lib ${CMAKE_EXTRA_LIBRARIES})

## -----------------------------------------------------------------------------
## Actions taken when all components have been found

if(LIBRT_INCLUDES AND LIBRT_LIBRARIES)
  set(LIBRT_FOUND TRUE)
else()
  if(LIBRT_FIND_VERBOSE)
    if(NOT LIBRT_INCLUDES)
      message(STATUS "Unable to find RT header files!")
    endif()
    if(NOT LIBRT_LIBRARIES)
      message(STATUS "Unable to find RT library files!")
    endif()
  endif()
endif()

if(LIBRT_FOUND)
  if(LIBRT_FIND_VERBOSE)
    message(STATUS "Found components for LIBRT")
    message(STATUS "LIBRT_INCLUDES = ${LIBRT_INCLUDES}")
    message(STATUS "LIBRT_LIBRARIES = ${LIBRT_LIBRARIES}")
  endif()
else()
  if(LIBRT_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find RT!")
  endif()
endif()

mark_as_advanced(LIBRT_FOUND LIBRT_LIBRARIES LIBRT_INCLUDES)
