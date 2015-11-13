# Project definitions
# 
project(test)

# 
# Executables
# 
add_executable(test-memory memory.cc)
add_executable(test-blockmemory blockmemory.cc)
add_executable(test-compress-map compress_map.cc)
add_executable(test-compress-serialize compress_serialize.cc)

if(STATIC_BINARIES)
  target_link_libraries(test-memory riss-lib-static coprocessor-lib-static)
  target_link_libraries(test-blockmemory riss-lib-static coprocessor-lib-static)
  target_link_libraries(test-compress-map riss-lib-static)
  target_link_libraries(test-compress-serialize riss-lib-static)
else()
  target_link_libraries(test-memory riss-lib-shared coprocessor-lib-shared)
  target_link_libraries(test-blockmemory riss-lib-shared coprocessor-lib-shared)
  target_link_libraries(test-compress-map riss-lib-shared)
  target_link_libraries(test-compress-serialize riss-lib-shared)
endif()
