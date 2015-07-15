# Project definitions
# 
project(risslibcheck)

# 
# Executables
# 
add_executable(risslibcheck Main.cc)

if(STATIC_BINARIES)
  # Because we got a circular dependency, we have have to specify riss-lib-* twice
  # or use the --start-group/--end-group notation which are only valid for the GNU
  # linker
  target_link_libraries(risslibcheck riss-lib-static coprocessor-lib-static riss-lib-static)
else()
  target_link_libraries(risslibcheck riss-lib-shared coprocessor-lib-shared)
endif()

# 
# Installation
# 
install(TARGETS   risslibcheck RUNTIME DESTINATION bin)
install(DIRECTORY risslibcheck
        DESTINATION include/risslibcheck
        FILES_MATCHING PATTERN "*.h")
