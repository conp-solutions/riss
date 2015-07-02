#
# Extract pcasso version information from git
#

set(TEMP ${OUTPUT}.tmp)

file(WRITE ${TEMP}
  "/*\n"
  " * This file was created automatically. Do not change it.\n"
  " * If you want to distribute the source code without\n"
  " * git, make sure you include this file in your bundle.\n"
  " */\n"
  "#include \"${PROJECT_NAME}/version.h\"\n"
  "\n"
  "const char* Pcasso::gitSHA1            = \"${GIT_SHA1}\";\n"
  "const char* Pcasso::gitDate            = \"${GIT_DATE}\";\n"
  "const char* Pcasso::signature          = \"${PROJECT_NAME} ${VERSION} build ${GIT_SHA1}\";\n"
)

# copy the file to the final header only if the version changes
# reduces needless rebuilds
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${TEMP}
  ${OUTPUT}
)

# Remove the temporary generated file
file(REMOVE ${TEMP})
