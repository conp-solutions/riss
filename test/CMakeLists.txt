# Project definitions
# 
project(test)

# 
# Executables
# 
add_executable(test-memory memory.cc)
add_executable(perf-compress compress_performance.cc)
add_executable(compress-serialize compress_serialize.cc)



if(STATIC_BINARIES)
  target_link_libraries(test-memory riss-lib-static coprocessor-lib-static)
  target_link_libraries(perf-compress riss-lib-static)
    target_link_libraries(compress-serialize riss-lib-static)
else()
  target_link_libraries(test-memory riss-lib-shared coprocessor-lib-shared)
  target_link_libraries(perf-compress riss-lib-shared)
    target_link_libraries(compress-serialize riss-lib-shared)
endif()
