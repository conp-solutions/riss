# Project definitions
# 
project(test)

# 
# Executables
# 
add_executable(test-memory memory.cc)

if(STATIC_BINARIES)
  target_link_libraries(test-memory riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(test-memory riss-lib-shared coprocessor-lib-shared)
endif()
