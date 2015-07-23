# Project definitions
# 
project(shiftbmc)

# 
# Executables
# 
add_executable(shiftbmc       shiftbmc.cc)
add_executable(shiftbmc-abc   shiftbmc.cc)


if(STATIC_BINARIES)
  # we have to declare "riss-lib-static" twice because of circular dependencies
  target_link_libraries(shiftbmc        aiger-lib-static riss-lib-static coprocessor-lib-static riss-lib-static pfolio-lib-static picosat-lib-static)
  target_link_libraries(shiftbmc-abc    aiger-lib-static riss-lib-static coprocessor-lib-static riss-lib-static pfolio-lib-static picosat-lib-static)
else()
  target_link_libraries(shiftbmc        riss-lib-shared coprocessor-lib-shared pfolio-lib-shared picosat-lib-shared aiger-lib-shared)
  target_link_libraries(shiftbmc-abc    riss-lib-shared coprocessor-lib-shared pfolio-lib-shared picosat-lib-shared aiger-lib-shared)
endif()

# 
# Installation
# 
install(TARGETS   shiftbmc RUNTIME DESTINATION bin)
install(DIRECTORY shiftbmc
        DESTINATION include/shiftbmc
        FILES_MATCHING PATTERN "*.h")
