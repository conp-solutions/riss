# Project definitions:
# 
project(shiftbmc)

# Dependencies:
# 

# Libraries:
# 
set(LIB_SOURCES
    aiger.c
    simpaig.c
    aigfuzzlayers.c)

add_library(aiger-lib-static STATIC ${LIB_SOURCES})
add_library(aiger-lib-shared SHARED ${LIB_SOURCES})


# Executables:
# 
add_executable(shiftbmc         shiftbmc.cc)
add_executable(shiftbmc-abc     shiftbmc.cc)

# aiger tools by Biere
add_executable(aig-plain-bmc    aig-plain-bmc.c)
add_executable(aigand     aigand.c)
add_executable(aigbmc     aigbmc.c)
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

if(STATIC_BINARIES)
  target_link_libraries(shiftbmc        riss-lib-static aiger-lib-static)
  target_link_libraries(shiftbmc-abc    riss-lib-static aiger-lib-static)
  target_link_libraries(aig-plain-bmc   aiger-lib-static)
  target_link_libraries(aigand          aiger-lib-static)
  target_link_libraries(aigbmc          aiger-lib-static)
  target_link_libraries(aigdd           aiger-lib-static)
  target_link_libraries(aigfuzz         aiger-lib-static)
  target_link_libraries(aiginfo         aiger-lib-static)
  target_link_libraries(aiginfo         aiger-lib-static)
  target_link_libraries(aigflip         aiger-lib-static)
  target_link_libraries(aigmiter        aiger-lib-static)
  target_link_libraries(aigmove         aiger-lib-static)
  target_link_libraries(aignm           aiger-lib-static)
  target_link_libraries(aigor           aiger-lib-static)
  target_link_libraries(aigreset        aiger-lib-static)
  target_link_libraries(aigsim          aiger-lib-static)
  target_link_libraries(aigstrip        aiger-lib-static)
  target_link_libraries(aigsplit        aiger-lib-static)
  target_link_libraries(aigtoaig        aiger-lib-static)
  target_link_libraries(aigtoblif       aiger-lib-static)
  target_link_libraries(aigtocnf        aiger-lib-static)
  target_link_libraries(aigtodot        aiger-lib-static)
  target_link_libraries(aigtosmv        aiger-lib-static)
  target_link_libraries(andtoaig        aiger-lib-static)
  target_link_libraries(aigunroll       aiger-lib-static)
  target_link_libraries(bliftoaig       aiger-lib-static)
  target_link_libraries(smvtoaig        aiger-lib-static)
  target_link_libraries(soltostim       aiger-lib-static)
  target_link_libraries(wrapstim        aiger-lib-static)
  target_link_libraries(abc-demo        aiger-lib-static)
else()
  target_link_libraries(shiftbmc        riss-lib-shared aiger-lib-shared)
  target_link_libraries(shiftbmc-abc    riss-lib-shared aiger-lib-shared)
  target_link_libraries(aig-plain-bmc   aiger-lib-shared)
  target_link_libraries(aigand          aiger-lib-shared)
  target_link_libraries(aigbmc          aiger-lib-shared)
  target_link_libraries(aigdd           aiger-lib-shared)
  target_link_libraries(aigfuzz         aiger-lib-shared)
  target_link_libraries(aiginfo         aiger-lib-shared)
  target_link_libraries(aiginfo         aiger-lib-shared)
  target_link_libraries(aigflip         aiger-lib-shared)
  target_link_libraries(aigmiter        aiger-lib-shared)
  target_link_libraries(aigmove         aiger-lib-shared)
  target_link_libraries(aignm           aiger-lib-shared)
  target_link_libraries(aigor           aiger-lib-shared)
  target_link_libraries(aigreset        aiger-lib-shared)
  target_link_libraries(aigsim          aiger-lib-shared)
  target_link_libraries(aigstrip        aiger-lib-shared)
  target_link_libraries(aigsplit        aiger-lib-shared)
  target_link_libraries(aigtoaig        aiger-lib-shared)
  target_link_libraries(aigtoblif       aiger-lib-shared)
  target_link_libraries(aigtocnf        aiger-lib-shared)
  target_link_libraries(aigtodot        aiger-lib-shared)
  target_link_libraries(aigtosmv        aiger-lib-shared)
  target_link_libraries(andtoaig        aiger-lib-shared)
  target_link_libraries(aigunroll       aiger-lib-shared)
  target_link_libraries(bliftoaig       aiger-lib-shared)
  target_link_libraries(smvtoaig        aiger-lib-shared)
  target_link_libraries(soltostim       aiger-lib-shared)
  target_link_libraries(wrapstim        aiger-lib-shared)
  target_link_libraries(abc-demo        aiger-lib-shared)
endif()


# Installation:
# 
install(TARGETS   ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${PROJECT_NAME}
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h")
