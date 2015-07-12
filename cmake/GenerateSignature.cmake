# This CMake script will executed to generate the project specific
# signature files
# 
# @author: Lucas Kahlert <lucas.kahlert@tu-dresden.de>

# the commit's SHA1, and whether the building workspace was dirty or not
execute_process(COMMAND "${GIT_EXECUTABLE}" describe --always --abbrev --dirty
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_SHA1
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# the date of the commit
execute_process(COMMAND "${GIT_EXECUTABLE}" log -1 --format=%ad --date=local
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_DATE
                OUTPUT_STRIP_TRAILING_WHITESPACE)

set(TEMP_FILE ${DESTINATION}.tmp)

# Let the project-specific CMake script build the version file
execute_process(COMMAND ${CMAKE_COMMAND} -D PROJECT_NAME=${PROJECT_NAME}
                                         -D OUTPUT=${TEMP_FILE}
                                         -D GIT_SHA1=${GIT_SHA1}
                                         -D GIT_DATE=${GIT_DATE}
                                         -D VERSION=${VERSION}
                                         -P ${SCRIPT})
# copy the generated version file only if the commit (Git SHA1) has changes
# reduces needless rebuilds
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                         ${TEMP_FILE}
                                         ${DESTINATION})
# Remove the temporary generated file
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${TEMP_FILE})
