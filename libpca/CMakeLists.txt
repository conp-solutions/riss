# Project definitions
# 
project(libpca)
version("1.2.11")

set(LIB_SOURCES pca.cc utils.cc)

# 
# Armadillo (static)
# 
include(ExternalProject)
# Adds armadillo library as build target. We use a dirty hack,
# to install the header files directly into the source folder
# (they are ignored by git)
ExternalProject_Add(armadillo-static
  URL http://downloads.sourceforge.net/project/arma/armadillo-6.500.5.tar.gz
  PREFIX "armadillo"
  CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF
             -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
             -DINSTALL_LIB_DIR=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
             # We install the header files into the original source files to
             # get IDEs working with the "#include"s. We do not want to install
             # the project system wide but only locally for this project.
             -DINSTALL_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/armadillo
  PATCH_COMMAND patch -N include/armadillo_bits/config.hpp ${CMAKE_CURRENT_SOURCE_DIR}/config.patch
)

# add static armadillo library
add_library(armadillo-lib-static STATIC IMPORTED)
set_target_properties(armadillo-lib-static PROPERTIES IMPORTED_LOCATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libarmadillo${CMAKE_STATIC_LIBRARY_SUFFIX})
add_dependencies(armadillo-lib-static armadillo-static)

# 
# libpca static
# 
add_library(libpca-static STATIC ${LIB_SOURCES})
set_target_properties(libpca-static PROPERTIES OUTPUT_NAME "pca")
target_link_libraries(libpca-static armadillo-lib-static)

# 
# Armadillo (shared)
# 
ExternalProject_Add(armadillo-shared
  URL http://downloads.sourceforge.net/project/arma/armadillo-6.500.5.tar.gz
  PREFIX "armadillo"
  CMAKE_ARGS -DBUILD_SHARED_LIBS=ON
             -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
             -DINSTALL_LIB_DIR=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
             -DINSTALL_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/armadillo
)

# add shared armadillo library
add_library(armadillo-lib-shared STATIC IMPORTED)
set_target_properties(armadillo-lib-shared PROPERTIES IMPORTED_LOCATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libarmadillo${CMAKE_SHARED_LIBRARY_SUFFIX})
add_dependencies(armadillo-lib-shared armadillo-shared)

# 
# libpca shared
# 
add_library(libpca-shared SHARED ${LIB_SOURCES})
set_target_properties(libpca-shared PROPERTIES OUTPUT_NAME "pca"
                                               VERSION ${VERSION}
                                               SOVERSION ${SOVERSION})
target_link_libraries(libpca-shared armadillo-lib-shared)

