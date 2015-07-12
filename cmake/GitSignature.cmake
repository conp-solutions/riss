# Generic CMake function for extracting version signatures from Git
# 
# @author: Lucas Kahlert <lucas.kahlert@tu-dresden.de>
# 
find_package(Git)

function(git_signature)
  # argument parsing
  set(oneValueArgs   DESTINATION SCRIPT VERSION PROJECT_NAME)
  set(multiValueArgs )

  cmake_parse_arguments(GIT_SIGNATURE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Debug output
  # message(STATUS "GIT_SIGNATURE_PROJECT_NAME: ${GIT_SIGNATURE_PROJECT_NAME}")
  # message(STATUS "GIT_SIGNATURE_DESTINATION:  ${GIT_SIGNATURE_DESTINATION}")
  # message(STATUS "GIT_SIGNATURE_SCRIPT:       ${GIT_SIGNATURE_SCRIPT}")

  # Check if a git executable was found on the system and if the source code is
  # under version control
  if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git/")
    message(STATUS "Generating signature ${GIT_SIGNATURE_PROJECT_NAME}-version from git")

    set_source_files_properties(${GIT_SIGNATURE_DESTINATION} PROPERTIES GENERATED TRUE)

    # Custom version target that is always built, because CMake is told, that the command
    # generating this file produces another file which is never written. Therefore this target
    # is always out of date
    add_custom_target(${GIT_SIGNATURE_PROJECT_NAME}-version
                      DEPENDS ${GIT_SIGNATURE_PROJECT_NAME}-always)


    # Tell CMake how to build the "always" file on which the target "version"
    # depends on. On this way we will also create the version file
    # 
    # With this order of output files, the version file depends on the "always" target. Therefore
    # the "version" target will be generated twice: once for the dependecy of the ${PROJECT_NAME}
    # target and another one for the ${PROJECT_NAME} target itself.
    # But if you swap the order, the version file will be always build -- regardless of your
    # temporary copying. Therefore the ${PROJECT_NAME} is out of date and needs to be rebuild.
    # This is a time consuming process and obscure builds, because there will be always be build
    # something, regardless of changes in the source code.
    add_custom_command(OUTPUT ${GIT_SIGNATURE_PROJECT_NAME}-always ${GIT_SIGNATURE_DESTINATION}
                       COMMENT "Generating ${GIT_SIGNATURE_PROJECT_NAME}-version"
                       COMMAND ${CMAKE_COMMAND} -D GIT_EXECUTABLE=${GIT_EXECUTABLE}
                                                -D CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
                                                -D PROJECT_NAME=${GIT_SIGNATURE_PROJECT_NAME}
                                                -D DESTINATION=${GIT_SIGNATURE_DESTINATION}
                                                -D VERSION=${GIT_SIGNATURE_VERSION}
                                                -D SCRIPT=${GIT_SIGNATURE_SCRIPT}
                                                -P ${CMAKE_SOURCE_DIR}/cmake/GenerateSignature.cmake)
  else()
    message(STATUS "${GIT_SIGNATURE_PROJECT_NAME}-version: Git not found or source code not under version control")
  endif()

endfunction()
