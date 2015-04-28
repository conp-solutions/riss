# Project definitions:
# 
project(qprocessor)

# Dependencies:
# 

# Executables:
# 
add_executable(${PROJECT_NAME} Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(${PROJECT_NAME} riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(${PROJECT_NAME} lib-riss-shared coprocessor-lib-shared)
endif()

# Installation:
# 
install(TARGETS   ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${PROJECT_NAME}
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h")
