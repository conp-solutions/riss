# Project definitions:
# 
# Use the global version of the whole tool box. 
project(riss)

# 
# Dependencies
# 
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(Librt REQUIRED)

include_directories(${ZLIB_INCLUDE_DIR})
include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
include_directories(${LIBRT_INCLUDES})


# 
# Libraries
# 
set(VERSION_CC ${CMAKE_CURRENT_SOURCE_DIR}/utils/version.cc)
set(LIB_SOURCES
    librissc.cc
    ipasir.cc
    core/CoreConfig.cc
    core/Solver.cc
    core/EnumerateMaster.cc
    simp/SimpSolver.cc
    utils/Compression.cc
    utils/SimpleGraph.cc
    utils/Options.cc
    utils/Statistics-mt.cc
    utils/System.cc
    ${VERSION_CC})

add_library(riss-lib-static STATIC ${LIB_SOURCES})
add_library(riss-lib-shared SHARED ${LIB_SOURCES})
add_library(riss-lib-object OBJECT ${LIB_SOURCES})
# This extra libraries are build for pcasso, because pcasso needs some
# extensions to the riss source code
add_library(riss-pcasso-lib-static STATIC ${LIB_SOURCES})
add_library(riss-pcasso-lib-shared SHARED ${LIB_SOURCES})

message(STATUS "Libs: ${ZLIB_LIBRARY} ${LIBRT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}")

target_link_libraries(riss-lib-shared z pthread rt)
target_link_libraries(riss-lib-static z pthread rt)
target_link_libraries(riss-pcasso-lib-shared z pthread rt)
target_link_libraries(riss-pcasso-lib-static z pthread rt)

set_target_properties(riss-lib-static PROPERTIES
                                      OUTPUT_NAME "riss")
set_target_properties(riss-lib-shared PROPERTIES
                                      OUTPUT_NAME "riss" 
                                      VERSION ${VERSION}
                                      SOVERSION ${SOVERSION})
set_target_properties(riss-pcasso-lib-static PROPERTIES
                                             OUTPUT_NAME "riss-pcasso"
                                             COMPILE_FLAGS "-DPCASSO")
set_target_properties(riss-pcasso-lib-shared PROPERTIES
                                             OUTPUT_NAME "riss-pcasso"
                                             COMPILE_FLAGS "-DPCASSO"
                                             VERSION ${VERSION}
                                             SOVERSION ${SOVERSION})

git_signature(PROJECT_NAME "riss"
              SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/utils/version.cmake
              VERSION ${VERSION}
              DESTINATION ${VERSION_CC})

# 
# Executables
# 
add_executable(riss-core core/Main.cc)
add_executable(riss-simp simp/Main.cc)

if(CLASSIFIER)
  set_target_properties(riss-core PROPERTIES COMPILE_FLAGS "-DCLASSIFIER")
  set_target_properties(riss-simp PROPERTIES COMPILE_FLAGS "-DCLASSIFIER")
endif()

# Create a symlink to the binary as shortcut
add_custom_command(TARGET  riss-core POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink riss-core ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/riss)
add_custom_command(TARGET  riss-simp POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink riss-simp ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/riss)

if(STATIC_BINARIES)
  target_link_libraries(riss-core riss-lib-static coprocessor-lib-static)
  target_link_libraries(riss-simp riss-lib-static coprocessor-lib-static)

  if(CLASSIFIER)
    target_link_libraries(riss-core classifier-lib-static libpca-static)
    target_link_libraries(riss-simp classifier-lib-static libpca-static)
   endif()
else()
  target_link_libraries(riss-core riss-lib-shared coprocessor-lib-shared)
  target_link_libraries(riss-simp riss-lib-shared coprocessor-lib-shared)

  if(CLASSIFIER)
    target_link_libraries(riss-core classifier-lib-shared libpca-shared)
    target_link_libraries(riss-simp classifier-lib-shared libpca-shared)
   endif()
endif()

# Installation:
# 
install(TARGETS riss-lib-static riss-lib-shared riss-pcasso-lib-static riss-pcasso-lib-shared
                riss-core riss-simp 
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY riss/mtl riss/utils riss/core riss/simp
        DESTINATION include/riss
        FILES_MATCHING PATTERN "*.h")
