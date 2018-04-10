cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project("riss-toolbox")

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

# Adds a subdirectories but only if it exists
function(add_component directory)
  if(EXISTS ${CMAKE_SOURCE_DIR}/${directory})
    add_subdirectory(${directory})
  endif()
endfunction()

execute_process(
  COMMAND git rev-list v7.1.0..HEAD --count
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_VERSION_STRING
  OUTPUT_STRIP_TRAILING_WHITESPACE
)


# Options:
# 
# Version of the whole tool box. Subprojects can define their own versions. If
# no version is specified, this "global" version will be used,
if(GIT_VERSION_STRING)
  set(GIT_VERSION_STRING  "7.1.${GIT_VERSION_STRING}")
  version(${GIT_VERSION_STRING})
else()
  version("7.1.0")
endif()

option(DRATPROOF       "Produce DRAFT-proofs (disabled by default)" OFF)
option(STATIC_BINARIES "Build fully statically linked binaries" ON)
option(WARNINGS        "Set verbose warning flags" ON)
option(PROFILING       "Compile for gprof profiling" OFF)
option(FPIC            "Build shared library with -fpic" OFF)
option(ERROR           "Turn warnings into errors" OFF)
option(QUIET           "Disable all #warnings" OFF)

# Print option settings
message(STATUS "Build type ${CMAKE_BUILD_TYPE}")
message(STATUS "Static binaries ${STATIC_BINARIES}")
message(STATUS "Verbose warnings ${WARNINGS}")
message(STATUS "Compile with -fpic ${FPIC}")
message(STATUS "Compile for gprof profiling ${PROFILING}")
message(STATUS "Quiet ${QUIET}")

# Link time optimization
SET(CMAKE_AR  "gcc-ar")
SET(CMAKE_RANLIB  "gcc-ranlib")
# Output directory definitions
# 
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)


# Tell CMake where to look for additional CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Link only to static libs (incl. libc and others)
if(STATIC_BINARIES)
  set(CMAKE_EXE_LINKER_FLAGS "-static")

  # Remove dynamic link flags
  set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic 
  set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS) 
  set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # remove -rdynamic 
  set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS) 
endif()

# tuning for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_COMPILER_IS_GNUCXX AND NOT PROFILING)
  set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -s")  # strip binary
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")  # strip shared libs
endif()

# proofs
message("-- DRAT proofs ${DRATPROOF}")
if(DRATPROOF)
    add_definitions(" -DDRATPROOF")
endif()

# 
# Dependencies
#
include_directories(${PROJECT_SOURCE_DIR})

find_package(Git)
include(GitSignature)

# Compile flags:
# 
add_definitions("-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -std=c++11 -g -DNDBG")

# Link time optimization command line parameters
set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -flto")     # link time optimization
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -flto")  # link time optimization
add_definitions("-flto")

if(ERRORS)
  message(STATUS "Treat warnings as errors")
  add_definitions("-Werror")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
  if(WARNINGS)
    message(STATUS "enable verbose compiler warnings")
    add_definitions("-Wall -Wextra -ffloat-store -Wno-unused-but-set-variable"
                    "-Wno-unused-variable -Wno-unused-function -Wno-unused-parameter"
                    "-Wno-sign-compare -Wno-parentheses -Wno-delete-non-virtual-dtor")
   endif()
   if(QUIET)
     message(STATUS "disable #warnings")
     add_definitions("-Wno-cpp")
   endif()
   if(PROFILING)
     message(STATUS "enable profiling")
     add_definitions("-pg -O1")
     SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -pg -O1" )
   endif()
endif()

if(FPIC)
  message(STATUS "enable -fpic")
  add_definitions("-fpic")

  # Combined riss and coprocessor library. This makes linking against riss with
  # the usage of coprocessor simpler (you only have to link against one lib).
  add_library(riss-coprocessor-lib-shared SHARED $<TARGET_OBJECTS:riss-lib-object> $<TARGET_OBJECTS:coprocessor-lib-object>)
  set_target_properties(riss-coprocessor-lib-shared PROPERTIES OUTPUT_NAME "riss-coprocessor")
  set_property(TARGET riss-coprocessor-lib-shared PROPERTY POSITION_INDEPENDENT_CODE ON)
  target_link_libraries(riss-coprocessor-lib-shared z pthread rt)
endif()


# Sub-Projects:
# 
# We use the CMakeLists.txt files in subdirectories, because you maybe want
# to include other components and a huge single top-level CMakeLists.txt would
# be very harsh to manage. With the subdirs we got a much more modular approach.
# 
add_component(riss/)
add_component(risslibcheck/)
add_component(coprocessor/)
add_component(pfolio/)
add_component(test/)
add_component(doc/)
add_component(scripts/)

# Combined riss and coprocessor library. This makes linking against riss with
# the usage of coprocessor simpler (you only have to link against one lib).
add_library(riss-coprocessor-lib-static STATIC $<TARGET_OBJECTS:riss-lib-object> $<TARGET_OBJECTS:coprocessor-lib-object>)
set_target_properties(riss-coprocessor-lib-static PROPERTIES OUTPUT_NAME "riss-coprocessor")
target_link_libraries(riss-coprocessor-lib-static z pthread rt)
