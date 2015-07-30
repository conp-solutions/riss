# Project definitions
# 
project(classifier)
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
    knn.cc
    methods.cc)

add_library(classifier-lib-static STATIC ${LIB_SOURCES})
add_library(classifier-lib-shared SHARED ${LIB_SOURCES})

# This extra libraries are build for pcasso, because pcasso needs some
# extensions to the riss source code
add_library(classifier-pcasso-lib-static STATIC ${LIB_SOURCES})
add_library(classifier-pcasso-lib-shared SHARED ${LIB_SOURCES})

target_link_libraries(classifier-lib-static libpca-static)
target_link_libraries(classifier-lib-shared libpca-shared)

target_link_libraries(classifier-pcasso-lib-static libpca-static)
target_link_libraries(classifier-pcasso-lib-shared libpca-shared)

set_target_properties(classifier-lib-static PROPERTIES
                                            OUTPUT_NAME "classifier")
set_target_properties(classifier-lib-shared PROPERTIES
                                            OUTPUT_NAME "classifier" 
                                            VERSION ${VERSION}
                                            SOVERSION ${SOVERSION})

set_target_properties(classifier-pcasso-lib-static PROPERTIES
                                                   OUTPUT_NAME "classifier-pcasso"
                                                   COMPILE_FLAGS "-DPCASSO")
set_target_properties(classifier-pcasso-lib-shared PROPERTIES
                                                   OUTPUT_NAME "classifier-pcasso"
                                                   COMPILE_FLAGS "-DPCASSO"
                                                   VERSION ${VERSION}
                                                   SOVERSION ${SOVERSION})

# 
# Executables
# 
add_executable(classifier Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(classifier classifier-lib-static riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(classifier classifier-lib-shared riss-lib-shared coprocessor-lib-shared)
endif()

# 
# Installation
# 
install(TARGETS classifier-lib-static classifier-lib-shared classifier
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY classifier
        DESTINATION include/classifier
        FILES_MATCHING PATTERN "*.h")
