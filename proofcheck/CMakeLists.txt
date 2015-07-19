# Project definitions
# 
project(proofcheck)

# 
# Libraries
# 
set(LIB_SOURCES
    BackwardChecker.cc
    BackwardVerificationWorker.cc
    ProofChecker.cc)


add_library(proofcheck-lib-static STATIC ${LIB_SOURCES})
add_library(proofcheck-lib-shared SHARED ${LIB_SOURCES})

set_target_properties(proofcheck-lib-static PROPERTIES
                                            OUTPUT_NAME "proofcheck"
                                            COMPILE_FLAGS "-DPCASSO")
set_target_properties(proofcheck-lib-shared PROPERTIES
                                            OUTPUT_NAME "proofcheck" 
                                            COMPILE_FLAGS "-DPCASSO"
                                            VERSION ${VERSION}
                                            SOVERSION ${SOVERSION})

# 
# Executables
# 
add_executable(proofcheck Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(proofcheck proofcheck-lib-static riss-lib-static coprocessor-lib-static)
else()
  target_link_libraries(proofcheck proofcheck-lib-shared riss-lib-shared coprocessor-lib-shared)
endif()

# 
# Installation
# 
install(TARGETS proofcheck-lib-static proofcheck-lib-shared proofcheck
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY proofcheck
        DESTINATION include/proofcheck
        FILES_MATCHING PATTERN "*.h")
