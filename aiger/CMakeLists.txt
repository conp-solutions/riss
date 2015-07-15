# Project definitions
# 
# aiger tools by Biere
project(aiger)
version("1.9.6")

# 
# Dependencies
# 
find_package(Threads)

include_directories(${THREADS_PTHREADS_INCLUDE_DIR})

# 
# Libraries
# 
set(LIB_SOURCES
    aiger.c
    simpaig.c
    aigfuzzlayers.c)

add_library(aiger-lib-static STATIC ${LIB_SOURCES})
add_library(aiger-lib-shared SHARED ${LIB_SOURCES})

set_target_properties(aiger-lib-static PROPERTIES
                                       OUTPUT_NAME "aiger")
set_target_properties(aiger-lib-shared PROPERTIES
                                       OUTPUT_NAME "aiger" 
                                       VERSION ${VERSION}
                                       SOVERSION ${SOVERSION})

# List of all executables
set(TOOLS aig-plain-bmc
          aigand
          aigbmc
          aigdd
          aigfuzz
          aiginfo
          aigflip
          aigmiter
          aigmove
          aignm
          aigor
          aigreset
          aigsim
          aigstrip
          aigsplit
          aigtoaig
          aigtoblif
          aigtocnf
          aigtodot
          aigtosmv
          andtoaig
          aigunroll
          bliftoaig
          smvtoaig
          soltostim
          wrapstim
          abc-demo)

# Only build aiger tools, if flag is set. There are very many executables cluttering
# up your make targets if you do not need them. 
if(AIGER-TOOLS)

  foreach(TOOL ${TOOLS})
    # Adds files without extension, because there are mixed C (.c) and C++ (.cc)
    # files together
    add_executable(${TOOL} "${TOOL}")
  endforeach()

  # catch-all target for all aiger executables
  add_custom_target(aiger-tools
                    COMMENT "Build all aiger tools"
                    DEPENDS ${TOOLS})

  target_link_libraries(abc-demo abc m dl readline ${CMAKE_THREAD_LIBS_INIT})

  if(STATIC_BINARIES)
    target_link_libraries(aigbmc         picosat-lib-static)
    target_link_libraries(aig-plain-bmc  picosat-lib-static)

    # link each executable
    foreach(TOOL ${TOOLS})
      target_link_libraries(${TOOL} aiger-lib-static)
    endforeach()

  else()
    target_link_libraries(aigbmc         picosat-lib-shared)
    target_link_libraries(aig-plain-bmc  picosat-lib-shared)

    # link each executable
    foreach(TOOL ${TOOLS})
      target_link_libraries(${TOOL} aiger-lib-shared)
    endforeach()

  endif()

  # installation of all tools
  install(TARGETS ${TOOLS} RUNTIME DESTINATION bin)
endif()

# installation of libraries
install(DIRECTORY aiger
        DESTINATION include/aiger
        FILES_MATCHING PATTERN "*.h")
