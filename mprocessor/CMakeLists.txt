# Project definitions:
# 
project(mprocessor)

# Dependencies:
# 

# Executables:
# 
add_executable(${PROJECT_NAME} Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(${PROJECT_NAME} coprocessor-lib-static riss-lib-static)
else()
  target_link_libraries(${PROJECT_NAME} coprocessor-lib-shared riss-lib-shared)
endif()

# Installation:
# 
install(TARGETS   ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${PROJECT_NAME}
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h")
