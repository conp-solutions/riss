cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

# Configurable options:
# 
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
# add_subdirectory(qprocessor/)
add_subdirectory(shiftbmc/)
add_subdirectory(aiger/)
# add_subdirectory(test/)
