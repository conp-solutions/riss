# Project definitions:
# 
# aiger tools by Biere
project(aiger)
version("1.9.6")

# Dependencies:
# 
find_package(Threads)

include_directories(${THREADS_PTHREADS_INCLUDE_DIR})

# Libraries:
# 
set(LIB_SOURCES
    aiger.c
    simpaig.c
    aigfuzzlayers.c)

add_library(${PROJECT_NAME}-lib-static STATIC ${LIB_SOURCES})
add_library(${PROJECT_NAME}-lib-shared SHARED ${LIB_SOURCES})

set_target_properties(${PROJECT_NAME}-lib-static PROPERTIES
                                                 OUTPUT_NAME "${PROJECT_NAME}")
set_target_properties(${PROJECT_NAME}-lib-shared PROPERTIES
                                                 OUTPUT_NAME "${PROJECT_NAME}" 
                                                 VERSION ${VERSION}
                                                 SOVERSION ${SOVERSION})


# Only build aiger tools, if flag is set. There are very many executables cluttering
# up your make targets if you do not need them. 
if(AIGER-TOOLS)

  # Executables:
  # 
  add_executable(aig-plain-bmc    aig-plain-bmc.c)
  add_executable(aigand     aigand.c)
  add_executable(aigbmc     aigbmc.cc)
  add_executable(aigdd      aigdd.c)
  add_executable(aigfuzz    aigfuzz.c)
  add_executable(aiginfo    aigjoin.c)
  add_executable(aigflip    aigflip.c)
  add_executable(aigmiter   aigmiter.c)
  add_executable(aigmove    aigmove.c)
  add_executable(aignm      aignm.c)
  add_executable(aigor      aigor.c)
  add_executable(aigreset   aigreset.c)
  add_executable(aigsim     aigsim.c)
  add_executable(aigstrip   aigstrip.c)
  add_executable(aigsplit   aigsplit.c)
  add_executable(aigtoaig   aigtoaig.c)
  add_executable(aigtoblif  aigtoblif.c)
  add_executable(aigtocnf   aigtocnf.c)
  add_executable(aigtodot   aigtodot.c)
  add_executable(aigtosmv   aigtosmv.c)
  add_executable(andtoaig   andtoaig.c)
  add_executable(aigunroll  aigunroll.c)
  add_executable(bliftoaig  bliftoaig.c)
  add_executable(smvtoaig   smvtoaig.c)
  add_executable(soltostim  soltostim.c)
  add_executable(wrapstim   wrapstim.c)
  add_executable(abc-demo   demo.c)

  # catch-all target for all aiger executables
  add_custom_target(aiger-tools
                    COMMENT "Build all aiger tools"
                    DEPENDS aig-plain-bmc
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

  target_link_libraries(aigbmc         picosat)
  target_link_libraries(aig-plain-bmc  picosat)
  target_link_libraries(abc-demo       abc m dl readline ${CMAKE_THREAD_LIBS_INIT})


  if(STATIC_BINARIES)
    target_link_libraries(aig-plain-bmc   ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigand          ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigbmc          ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigdd           ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigfuzz         ${PROJECT_NAME}-lib-static)
    target_link_libraries(aiginfo         ${PROJECT_NAME}-lib-static)
    target_link_libraries(aiginfo         ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigflip         ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigmiter        ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigmove         ${PROJECT_NAME}-lib-static)
    target_link_libraries(aignm           ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigor           ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigreset        ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigsim          ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigstrip        ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigsplit        ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigtoaig        ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigtoblif       ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigtocnf        ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigtodot        ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigtosmv        ${PROJECT_NAME}-lib-static)
    target_link_libraries(andtoaig        ${PROJECT_NAME}-lib-static)
    target_link_libraries(aigunroll       ${PROJECT_NAME}-lib-static)
    target_link_libraries(bliftoaig       ${PROJECT_NAME}-lib-static)
    target_link_libraries(smvtoaig        ${PROJECT_NAME}-lib-static)
    target_link_libraries(soltostim       ${PROJECT_NAME}-lib-static)
    target_link_libraries(wrapstim        ${PROJECT_NAME}-lib-static)
    target_link_libraries(abc-demo        ${PROJECT_NAME}-lib-static)
  else()
    target_link_libraries(aig-plain-bmc   ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigand          ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigbmc          ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigdd           ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigfuzz         ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aiginfo         ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aiginfo         ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigflip         ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigmiter        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigmove         ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aignm           ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigor           ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigreset        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigsim          ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigstrip        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigsplit        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigtoaig        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigtoblif       ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigtocnf        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigtodot        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigtosmv        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(andtoaig        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(aigunroll       ${PROJECT_NAME}-lib-shared)
    target_link_libraries(bliftoaig       ${PROJECT_NAME}-lib-shared)
    target_link_libraries(smvtoaig        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(soltostim       ${PROJECT_NAME}-lib-shared)
    target_link_libraries(wrapstim        ${PROJECT_NAME}-lib-shared)
    target_link_libraries(abc-demo        ${PROJECT_NAME}-lib-shared)
  endif()

  # installation of all tools
  install(TARGETS aig-plain-bmc
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
                  abc-demo
          RUNTIME DESTINATION bin)
endif()

# installation of libraries
install(DIRECTORY ${PROJECT_NAME}
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h")
