#
# Extract pcasso version information from git
#
find_package(Git)

function(git_signature)
  # set(options        OPTIONAL    FAST)
  set(oneValueArgs   DESTINATION SCRIPT VERSION)
  set(multiValueArgs TARGETS)

  cmake_parse_arguments(GIT_SIGNATURE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

  message(STATUS "GIT_SIGNATURE_DESTINATION: ${GIT_SIGNATURE_DESTINATION}")
  message(STATUS "GIT_SIGNATURE_SCRIPT:      ${GIT_SIGNATURE_SCRIPT}")
  message(STATUS "GIT_SIGNATURE_TARGETS:     ${GIT_SIGNATURE_TARGETS}")


  if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git/")
    message(STATUS "Build signature from git")

    # the commit's SHA1, and whether the building workspace was dirty or not
    execute_process(COMMAND
                    "${GIT_EXECUTABLE}" describe --always --abbrev --dirty
                    WORKING_DIRECTORY "${GIT_DIR}"
                    OUTPUT_VARIABLE GIT_SHA1
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    # the date of the commit
    execute_process(COMMAND
                    "${GIT_EXECUTABLE}" log -1 --format=%ad --date=local
                    WORKING_DIRECTORY "${GIT_DIR}"
                    OUTPUT_VARIABLE GIT_DATE
                    OUTPUT_STRIP_TRAILING_WHITESPACE)


    # Custom version target that is always built, because it DEPENDS on a file (string)
    # that does not exist
    add_custom_target(${PROJECT_NAME}-version
                      # build_version_file is nothing more than a unique string
                      DEPENDS ${PROJECT_NAME}-build_version_file)

    # Tell CMake how to build the "build_version_file" on which the target "version"
    # depends on. On this way we will also create the version.cc file
    add_custom_command(OUTPUT ${PROJECT_NAME}-build_version_file ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/version.cc
                       COMMENT "Build ${PROJECT_NAME}-version"
                       COMMAND ${CMAKE_COMMAND} -D PROJECT_NAME=${PROJECT_NAME}
                                                -D OUTPUT=${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/version.cc
                                                -D GIT_SHA1=${GIT_SHA1}
                                                -D GIT_DATE=${GIT_DATE}
                                                -D VERSION=${GIT_SIGNATURE_VERSION}
                                                -P ${GIT_SIGNATURE_SCRIPT})

    # explicitly say that the libraries depends on the version
    foreach(TARGET ${GIT_SIGNATURE_TARGETS})
      message(STATUS "Add ${PROJECT_NAME}-version for ${TARGET}")
      add_dependencies(${TARGET} ${PROJECT_NAME}-version)
    endforeach()

  endif()



# # Generate version source file
# #
# set(TEMP ${OUTPUT}.tmp)

# file(WRITE ${TEMP}
#   "/*\n"
#   " * This file was created automatically. Do not change it.\n"
#   " * If you want to distribute the source code without\n"
#   " * git, make sure you include this file in your bundle.\n"
#   " */\n"
#   "#include \"${PROJECT_NAME}/version.h\"\n"
#   "\n"
#   "const char* Pcasso::gitSHA1            = \"${GIT_SHA1}\";\n"
#   "const char* Pcasso::gitDate            = \"${GIT_DATE}\";\n"
#   "const char* Pcasso::signature          = \"${PROJECT_NAME} ${VERSION} build ${GIT_SHA1}\";\n"
# )

# # copy the file to the final header only if the version changes
# # reduces needless rebuilds
# execute_process(
#   COMMAND ${CMAKE_COMMAND} -E copy_if_different
#   ${TEMP}
#   ${OUTPUT}
# )

# # Remove the temporary generated file
# file(REMOVE ${TEMP})

endfunction()
