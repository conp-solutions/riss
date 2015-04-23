# Project definitions:
# 
project(risslibcheck)

# Dependencies:
# 

# Executables:
# 
add_executable(${PROJECT_NAME} Main.cc)

if(STATIC_BINARIES)
  # Because we got a circular dependency, we have have to specify riss-lib-* twice
  # or use the --start-group/--end-group notation which are only valid for the GNU
  # linker
  target_link_libraries(${PROJECT_NAME} riss-lib-static coprocessor-lib-static riss-lib-static)
else()
  target_link_libraries(${PROJECT_NAME} riss-lib-shared coprocessor-lib-shared)
endif()


# Installation:
# 
install(TARGETS   ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${PROJECT_NAME}
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h")