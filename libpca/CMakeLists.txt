# Project definitions
# 
project(libpca)
version("1.2.11")

set(LIB_SOURCES pca.cc utils.cc)

# 
# Armadillo
# 
include(ExternalProject)
if(STATIC_BINARIES)
  # Adds armadillo library as build target. We use a dirty hack,
  # to install the header files directly into the source folder
  # (they are ignored by git)
  ExternalProject_Add(armadillo-lib-static
    URL http://sourceforge.net/projects/arma/files/armadillo-5.200.2.tar.gz
    PREFIX "armadillo"
    CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF
               -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
               -DINSTALL_LIB_DIR=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
               # We install the header files into the original source files to
               # get IDEs working with the "#include"s. We do not want to install
               # the project system wide but only locally for this project.
               -DINSTALL_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/armadillo
  )

  # 
  # libpca static
  # 
  add_library(libpca-static STATIC ${LIB_SOURCES})
  set_target_properties(libpca-static PROPERTIES OUTPUT_NAME "pca")
  target_link_libraries(libpca-static "armadillo")
  add_dependencies(libpca-static armadillo-lib-static)
else()
  ExternalProject_Add(armadillo-lib-shared
    URL http://sourceforge.net/projects/arma/files/armadillo-5.200.2.tar.gz
    PREFIX "armadillo"
    CMAKE_ARGS -DBUILD_SHARED_LIBS=ON
               -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
               -DINSTALL_LIB_DIR=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
               -DINSTALL_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/armadillo
  )
  
  # 
  # libpca shared
  # 
  add_library(libpca-shared SHARED ${LIB_SOURCES})
  set_target_properties(libpca-shared PROPERTIES OUTPUT_NAME "pca"
                                                 VERSION ${VERSION}
                                                 SOVERSION ${SOVERSION})
  target_link_libraries(libpca-shared "armadillo")
  add_dependencies(libpca-shared armadillo-lib-shared)
endif()

# Just for convencience that we you can use #include <armadillo> instead
# of #include "armadillo/armadillo"
include_directories(${CMAKE_SOURCE_DIR}/armadillo)
