# Project definitions:
# 
project(coprocessor)

# Project version based on http://stackoverflow.com/a/18658684/2467158
set(VERSION "3.0.0")

string(REPLACE "." ";" VERSION_LIST ${VERSION})
list(GET VERSION_LIST 0 VERSION_MAJOR)
list(GET VERSION_LIST 1 VERSION_MINOR)
list(GET VERSION_LIST 2 VERSION_PATCH)

set(SOVERSION ${VERSION_MAJOR})


# Dependencies:
# 


# Libraries:
# 
set(LIB_SOURCES
    BoundedVariableElimination.cc
    BoundedVariableEliminationParallel.cc
    Bva.cc
    CP3Config.cc
    Circuit.cc
    ClauseElimination.cc
    Coprocessor.cc
    Dense.cc
    Entailed.cc
    EquivalenceElimination.cc
    Experimental.cc
    FourierMotzkin.cc
    HiddenTautologyElimination.cc
    LiteralAddition.cc
    OutputFormula.cc
    Probing.cc
    Propagation.cc
    Resolving.cc
    Rewriter.cc
    Shuffler.cc
    Subsumption.cc
    Symmetry.cc
    TwoSAT.cc
    Unhiding.cc
    bce.cc
    libcoprocessor.cc
    rate.cc
    sls.cc
    xor.cc)

add_library(${PROJECT_NAME}-lib-static STATIC ${LIB_SOURCES})
add_library(${PROJECT_NAME}-lib-shared SHARED ${LIB_SOURCES})

set_target_properties(${PROJECT_NAME}-lib-static PROPERTIES
                                                 OUTPUT_NAME "${PROJECT_NAME}")
set_target_properties(${PROJECT_NAME}-lib-shared PROPERTIES
                                                 OUTPUT_NAME "${PROJECT_NAME}" 
                                                 VERSION ${VERSION}
                                                 SOVERSION ${SOVERSION})


# Executables:
# 
add_executable(${PROJECT_NAME} Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib-static)
  target_link_libraries(${PROJECT_NAME} riss-lib-static)
else()
  target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib-shared)
  target_link_libraries(${PROJECT_NAME} riss-lib-shared)
endif()

# Installation:
# 
install(TARGETS ${PROJECT_NAME}-lib-static ${PROJECT_NAME}-lib-shared ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY ${PROJECT_NAME}
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h")