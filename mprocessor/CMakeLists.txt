# Project definitions
# 
project(mprocessor)

#
# Dependencies
# 

#
# Executables
# 
add_executable(mprocessor Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(mprocessor coprocessor-lib-static riss-lib-static)
else()
  target_link_libraries(mprocessor coprocessor-lib-shared riss-lib-shared)
endif()

#
# Installation
# 
install(TARGETS   mprocessor RUNTIME DESTINATION bin)
install(DIRECTORY mprocessor
        DESTINATION include/mprocessor
        FILES_MATCHING PATTERN "*.h")
