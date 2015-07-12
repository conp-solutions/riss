# Project definitions:
# 
# Use the global version of the whole tool box. 
project(riss)

option(DRATPROOF "Produce DRAFT-proofs (disabled by default)" OFF)


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
    core/CoreConfig.cc
    core/Solver.cc
    simp/SimpSolver.cc
    utils/Options.cc
    utils/Statistics-mt.cc
    utils/System.cc
    librissc.cc
    ${VERSION_CC})

add_library(${PROJECT_NAME}-lib-static STATIC ${LIB_SOURCES})
add_library(${PROJECT_NAME}-lib-shared SHARED ${LIB_SOURCES})
# This extra libraries are build for pcasso, because pcasso needs some
# extensions to the riss source code
add_library(${PROJECT_NAME}-pcasso-lib-static STATIC ${LIB_SOURCES})
add_library(${PROJECT_NAME}-pcasso-lib-shared SHARED ${LIB_SOURCES})

message(STATUS "Libs: ${ZLIB_LIBRARY} ${LIBRT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}")

target_link_libraries(${PROJECT_NAME}-lib-shared z pthread rt)
target_link_libraries(${PROJECT_NAME}-lib-static z pthread rt)
target_link_libraries(${PROJECT_NAME}-pcasso-lib-shared z pthread rt)
target_link_libraries(${PROJECT_NAME}-pcasso-lib-static z pthread rt)

set_target_properties(${PROJECT_NAME}-lib-static PROPERTIES
                                                 OUTPUT_NAME "${PROJECT_NAME}")
set_target_properties(${PROJECT_NAME}-lib-shared PROPERTIES
                                                 OUTPUT_NAME "${PROJECT_NAME}" 
                                                 VERSION ${VERSION}
                                                 SOVERSION ${SOVERSION})
set_target_properties(${PROJECT_NAME}-pcasso-lib-static PROPERTIES
                                                        OUTPUT_NAME "${PROJECT_NAME}-pcasso"
                                                        COMPILE_FLAGS "-DPCASSO")
set_target_properties(${PROJECT_NAME}-pcasso-lib-shared PROPERTIES
                                                        OUTPUT_NAME "${PROJECT_NAME}-pcasso"
                                                        COMPILE_FLAGS "-DPCASSO"
                                                        VERSION ${VERSION}
                                                        SOVERSION ${SOVERSION})

git_signature(PROJECT_NAME ${PROJECT_NAME}
              SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/utils/version.cmake
              VERSION ${VERSION}
              DESTINATION ${VERSION_CC})

# 
# Executables
# 
add_executable(${PROJECT_NAME}-core core/Main.cc)
add_executable(${PROJECT_NAME}-simp simp/Main.cc)

message("-- DRAT proofs ${DRATPROOF}")
if(DRATPROOF)
    set_target_properties(${PROJECT_NAME}-core PROPERTIES COMPILE_FLAGS "-DDRATPROOF")
    set_target_properties(${PROJECT_NAME}-simp PROPERTIES COMPILE_FLAGS "-DDRATPROOF")
endif()

# Create a symlink to the binary as shortcut
add_custom_command(TARGET  ${PROJECT_NAME}-core POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_NAME}-core ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME})
add_custom_command(TARGET  ${PROJECT_NAME}-simp POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_NAME}-simp ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME})

if(STATIC_BINARIES)
  target_link_libraries(${PROJECT_NAME}-core ${PROJECT_NAME}-lib-static coprocessor-lib-static)
  target_link_libraries(${PROJECT_NAME}-simp ${PROJECT_NAME}-lib-static coprocessor-lib-static)
else()
  target_link_libraries(${PROJECT_NAME}-core ${PROJECT_NAME}-lib-shared coprocessor-lib-shared)
  target_link_libraries(${PROJECT_NAME}-simp ${PROJECT_NAME}-lib-shared coprocessor-lib-shared)
endif()

# Installation:
# 
install(TARGETS ${PROJECT_NAME}-lib-static ${PROJECT_NAME}-lib-shared ${PROJECT_NAME}-pcasso-lib-static ${PROJECT_NAME}-pcasso-lib-shared
                ${PROJECT_NAME}-core ${PROJECT_NAME}-simp 
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY ${PROJECT_NAME}/mtl ${PROJECT_NAME}/utils ${PROJECT_NAME}/core ${PROJECT_NAME}/simp
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h")
