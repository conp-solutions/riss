# Project definitions
# 
project(pfolio)

#
# Libraries
# 
set(LIB_SOURCES
    PSolver.cc
    PfolioConfig.cc
    libprissc.cc)

add_library(pfolio-lib-static STATIC ${LIB_SOURCES})
add_library(pfolio-lib-shared SHARED ${LIB_SOURCES})

set_target_properties(pfolio-lib-static PROPERTIES
                                        OUTPUT_NAME "pfolio")
set_target_properties(pfolio-lib-shared PROPERTIES
                                        OUTPUT_NAME "pfolio" 
                                        VERSION ${VERSION}
                                        SOVERSION ${SOVERSION})

#
# Executables
# 
add_executable(pfolio Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(pfolio pfolio-lib-static riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(pfolio pfolio-lib-shared riss-lib-shared coprocessor-lib-shared)
endif()

# Create a symlink for other names
add_custom_command(TARGET  pfolio POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink pfolio ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/priss)

#
# Installation
# 
install(TARGETS pfolio-lib-static pfolio-lib-shared pfolio
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY pfolio
        DESTINATION include/pfolio
        FILES_MATCHING PATTERN "*.h")
