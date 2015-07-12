# Project definitions
# 
project(qprocessor)

# 
# Dependencies
# 

# 
# Executables
# 
add_executable(qprocessor Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(qprocessor riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(qprocessor riss-lib-shared coprocessor-lib-shared)
endif()

# 
# Installation
# 
install(TARGETS   qprocessor RUNTIME DESTINATION bin)
install(DIRECTORY qprocessor
        DESTINATION include/qprocessor
        FILES_MATCHING PATTERN "*.h")
