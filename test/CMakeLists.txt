# Project definitions:
# 
project(test)


# Dependencies:
# 


# Executables:
# 
add_executable(${PROJECT_NAME}-memory memory.cc)

if(STATIC_BINARIES)
  target_link_libraries(${PROJECT_NAME}-memory riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(${PROJECT_NAME}-memory riss-lib-shared coprocessor-lib-shared)
endif()
