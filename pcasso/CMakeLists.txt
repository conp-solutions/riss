# Project definitions:
# 
project(pcasso)


# Dependencies:
# 


# Libraries:
# 
set(LIB_SOURCES
    LevelPool.cc
    LookaheadSplitting.cc
    Master.cc
    PartitionTree.cc
    SolverPT.cc
    shared_pool.cc
    vsidsSplitting.cc)


add_library(${PROJECT_NAME}-lib-static STATIC ${LIB_SOURCES})
add_library(${PROJECT_NAME}-lib-shared SHARED ${LIB_SOURCES})

set_target_properties(${PROJECT_NAME}-lib-static PROPERTIES
                                                 OUTPUT_NAME "${PROJECT_NAME}"
                                                 COMPILE_FLAGS "-DPCASSO")
set_target_properties(${PROJECT_NAME}-lib-shared PROPERTIES
                                                 OUTPUT_NAME "${PROJECT_NAME}" 
                                                 COMPILE_FLAGS "-DPCASSO"
                                                 VERSION ${VERSION}
                                                 SOVERSION ${SOVERSION})

# Executables:
# 
add_executable(${PROJECT_NAME} Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib-static riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib-shared riss-lib-shared coprocessor-lib-shared)
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
