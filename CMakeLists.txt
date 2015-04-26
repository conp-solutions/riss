cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

# Auxiliary functions:
# 
# Splits a string of format "<major>.<minor>.<path>" into
# the three subcomponents
# based on http://stackoverflow.com/a/18658684/2467158
function(version version_string)
  set(VERSION ${version_string} PARENT_SCOPE)

  string(REPLACE "." ";" VERSION_LIST ${version_string})
  list(GET VERSION_LIST 0 VERSION_MAJOR)
  list(GET VERSION_LIST 1 VERSION_MINOR)
  list(GET VERSION_LIST 2 VERSION_PATCH)

  # Propagate results
  set(VERSION_MAJOR ${VERSION_MAJOR} PARENT_SCOPE)
  set(VERSION_MINOR ${VERSION_MINOR} PARENT_SCOPE)
  set(VERSION_PATCH ${VERSION_PATCH} PARENT_SCOPE)

  set(SOVERSION ${VERSION_MAJOR} PARENT_SCOPE)
endfunction()


# Defintions:
# 
# Version of the whole tool box. Subprojects can define their own versions. If
# no version is specified, this "global" version will be used,
version("5.4.0")

option(STATIC_BINARIES "Link binaries statically." ON)


# Output directory definitions
# 
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)


# Dependencies:
# 
include_directories(${PROJECT_SOURCE_DIR})


# Compile flags:
# 
add_definitions(-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)


# Sub-Projects:
# 
# We use the CMakeLists.txt files in subdirectories, because you maybe want
# to include other components and a huge single top-level CMakeLists.txt would
# be very harsh to manage. With the subdirs we got a much more modular approach.
# 
add_subdirectory(riss/)
add_subdirectory(risslibcheck/)
add_subdirectory(classifier/)
add_subdirectory(coprocessor/)
add_subdirectory(mprocessor/)
# add_subdirectory(pcasso/)
# add_subdirectory(pfolio/)
# add_subdirectory(proofcheck/)
add_subdirectory(qprocessor/)
add_subdirectory(shiftbmc/)
add_subdirectory(aiger/)
# add_subdirectory(test/)
