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
    Main.cc
    Configurations.cc
    FeaturesWriter.cc
    CNFClassifier.cc
    BipartiteGraph.cc
    WekaDataset.cc)

add_library(${PROJECT_NAME}-lib-static STATIC ${LIB_SOURCES})
add_library(${PROJECT_NAME}-lib-shared SHARED ${LIB_SOURCES})

set_target_properties(${PROJECT_NAME}-lib-static PROPERTIES
                                                 OUTPUT_NAME "${PROJECT_NAME}")
set_target_properties(${PROJECT_NAME}-lib-shared PROPERTIES
                                                 OUTPUT_NAME "${PROJECT_NAME}" 
                                                 VERSION ${VERSION}
                                                 SOVERSION ${SOVERSION})

if(STATIC_BINARIES)
  set(ARMADILLO_SHARED "-DBUILD_SHARED_LIBS=OFF")
else()
  set(ARMADILLO_SHARED "-DBUILD_SHARED_LIBS=ON")
endif()


# Adds armadillo library as build target. We use a dirty hack,
# to install the header files directly into the source folder
# (they are ignored by git)
include(ExternalProject)
ExternalProject_Add(armadillo
  URL http://sourceforge.net/projects/arma/files/armadillo-5.200.2.tar.gz
  PREFIX "armadillo"
  CMAKE_ARGS ${ARMADILLO_SHARED}
             -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
             -DINSTALL_LIB_DIR=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
             -DINSTALL_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/armadillo
) 


# 
# Executables
# 
add_executable(${PROJECT_NAME} Main.cc)


if(STATIC_BINARIES)
  target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib-static riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib-shared riss-lib-shared coprocessor-lib-shared -lpca -larmadillo)
endif()

# 
# Installation
# 
install(TARGETS ${PROJECT_NAME}-lib-static ${PROJECT_NAME}-lib-shared ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY ${PROJECT_NAME}
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h")
