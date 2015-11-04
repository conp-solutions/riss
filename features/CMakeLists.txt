# Project definitions
# 
project(features)
version("1.0.0")

# 
# Dependencies
# 
find_package(ZLIB)

include_directories(${ZLIB_INCLUDE_DIR})

# 
# Libraries
# 
set(LIB_SOURCES
    Classifier.cc
    SequenceStatistics.cc
    Graph.cc
    Configurations.cc
    FeaturesWriter.cc
    CNFClassifier.cc
    BipartiteGraph.cc
    WekaDataset.cc
    methods.cc)

add_library(features-lib-static STATIC ${LIB_SOURCES})
add_library(features-lib-shared SHARED ${LIB_SOURCES})

# This extra libraries are build for pcasso, because pcasso needs some
# extensions to the riss source code
add_library(features-pcasso-lib-static STATIC ${LIB_SOURCES})
add_library(features-pcasso-lib-shared SHARED ${LIB_SOURCES})

target_link_libraries(features-lib-static)
target_link_libraries(features-lib-shared)

target_link_libraries(features-pcasso-lib-static)
target_link_libraries(features-pcasso-lib-shared)

set_target_properties(features-lib-static PROPERTIES
                                            OUTPUT_NAME "features")
set_target_properties(features-lib-shared PROPERTIES
                                            OUTPUT_NAME "features" 
                                            VERSION ${VERSION}
                                            SOVERSION ${SOVERSION})

set_target_properties(features-pcasso-lib-static PROPERTIES
                                                   OUTPUT_NAME "features-pcasso"
                                                   COMPILE_FLAGS "-DPCASSO")
set_target_properties(features-pcasso-lib-shared PROPERTIES
                                                   OUTPUT_NAME "features-pcasso"
                                                   COMPILE_FLAGS "-DPCASSO"
                                                   VERSION ${VERSION}
                                                   SOVERSION ${SOVERSION})

# 
# Executables
# 
add_executable(features Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(features features-lib-static riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(features features-lib-shared riss-lib-shared coprocessor-lib-shared)
endif()

# 
# Installation
# 
install(TARGETS features-lib-static features-lib-shared features
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY features
        DESTINATION include/features
        FILES_MATCHING PATTERN "*.h")
