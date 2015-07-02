cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

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


# Options:
# 
# Version of the whole tool box. Subprojects can define their own versions. If
# no version is specified, this "global" version will be used,
version("5.0.2")

option(STATIC_BINARIES "Build fully statically linked binaries" ON)
option(PCASSO          "adapt assertions to algorithm used in pcasso" OFF)
option(SHIFTBMC        "Include agier and shiftbmc build targets" OFF)
option(AIGER-TOOLS     "Include build targets for all agier executables" OFF)
option(WARNINGS        "Set verbose warning flags" OFF)

# Print option settings
message(STATUS "Static binaries ${STATIC_BINARIES}")
message(STATUS "ShiftBMC ${SHIFTBMC}")
message(STATUS "aiger-tools ${AIGER-TOOLS}")
message(STATUS "Verbose warnings ${WARNINGS}")

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
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_EXE_LINKER_FLAGS    "-s")  # strip binary
  set(CMAKE_SHARED_LINKER_FLAGS "-s")  # strip shared libs
endif()


# Dependencies:
# 
include_directories(${PROJECT_SOURCE_DIR})

# Compile flags:
# 
add_definitions("-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -std=c++11 -g")

if(CMAKE_COMPILER_IS_GNUCXX AND WARNINGS)
  message(STATUS "enable verbose compiler warnings")
  add_definitions("-Wall -Wextra -ffloat-store -Wno-unused-but-set-variable"
                  "-Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare"
                  "-Wno-parentheses")
endif()

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
add_subdirectory(externalTools/)
add_subdirectory(mprocessor/)
add_subdirectory(pcasso/)
add_subdirectory(pfolio/)
add_subdirectory(proofcheck/)
add_subdirectory(qprocessor/)
add_subdirectory(test/)
add_subdirectory(doc/)
add_subdirectory(scripts/)

if(SHIFTBMC)
  add_subdirectory(shiftbmc/)
endif()

# Subdirectories are not allowed to be included twice,
# therefore we have two cases for the aiger-tools to
# be imported.
if(AIGER-TOOLS OR SHIFTBMC)
  add_subdirectory(aiger/)
endif()

if(AIGER-TOOLS)
  ExternalProject_Add(abc
    PREFIX       "abc"
    TMP_DIR      "abc/tmp"
    STAMP_DIR    "abc/stamp"
    DOWNLOAD_DIR "abc/tmp"
    SOURCE_DIR   "abc/src/abc"
    BINARY_DIR   "abc/src/abc"

    # Download step
    URL "https://bitbucket.org/alanmi/abc/get/default.tar.bz2"
    DOWNLOAD_NAME abc.tar.bz2

    # There is no configure script
    CONFIGURE_COMMAND ""

    BUILD_COMMAND make libabc.a

    # Install step
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy "libabc.a" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}

    # Logging
    LOG_BUILD 1
  )
  add_library(abc-lib-static STATIC IMPORTED)
  set_property(TARGET abc-lib-static PROPERTY IMPORTED_LOCATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libabc${CMAKE_STATIC_LIBRARY_SUFFIX})
  add_dependencies(abc-lib-static abc)

endif()


# Combined riss and coprocessor library. This makes linking against riss with
# the usage of coprocessor simpler (you only have to link against one lib).
include(MergeStaticLibs)
merge_static_libs(riss-coprocessor riss-lib-static coprocessor-lib-static)
