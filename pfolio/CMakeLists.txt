# Project definitions:
# 
project(pfolio)


# Libraries:
# 
set(LIB_SOURCES
    PSolver.cc
    PfolioConfig.cc
    libprissc.cc)

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
  target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib-static riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib-shared riss-lib-static coprocessor-lib-shared)
endif()

# Create a symlink for other names
add_custom_command(TARGET  ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_NAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/priss)


# Installation:
# 
install(TARGETS ${PROJECT_NAME}-lib-static ${PROJECT_NAME}-lib-shared ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY ${PROJECT_NAME}
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h")
