# Project definitions:
# 
project(shiftbmc)

# Dependencies:
# 
set(PICOSAT_LIB "/usr/local/lib" CACHE FILEPATH "Path to the directory containing the PicoSAT library")
link_directories(${PICOSAT_LIB})
include_directories(${PICOSAT_LIB})


# Libraries:
# 

# Executables:
# 
add_executable(shiftbmc         shiftbmc.cc)
add_executable(shiftbmc-abc     shiftbmc.cc)


if(STATIC_BINARIES)
  target_link_libraries(shiftbmc        riss-lib-static aiger-lib-static)
  target_link_libraries(shiftbmc-abc    riss-lib-static aiger-lib-static)
else()
  target_link_libraries(shiftbmc        riss-lib-shared aiger-lib-shared)
  target_link_libraries(shiftbmc-abc    riss-lib-shared aiger-lib-shared)
endif()


# Installation:
# 
install(TARGETS   ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${PROJECT_NAME}
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h")
