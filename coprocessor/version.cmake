# Extract version information from git
# 
find_package(Git)

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

# Generate version source file
# 
set(VERSION_CC   ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/version.cc)
set(VERSION_TEMP ${VERSION_CC}.tmp)

file(WRITE ${VERSION_TEMP}
  "/*\n"
  " * This file was created automatically. Do not change it.\n"
  " * If you want to distribute the source code without\n"
  " * git, make sure you include this file in your bundle.\n"
  " */\n"
  "#include \"${PROJECT_NAME}/version.h\"\n"
  "\n"
  "const char* Coprocessor::gitSHA1            = \"${GIT_SHA1}\";\n"
  "const char* Coprocessor::gitDate            = \"${GIT_DATE}\";\n"
  "const char* Coprocessor::coprocessorVersion = \"${VERSION}\";\n"
  "const char* Coprocessor::signature          = \"${PROJECT_NAME} ${VERSION} build ${GIT_SHA1}\";\n"
)

# copy the file to the final header only if the version changes
# reduces needless rebuilds
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${VERSION_TEMP}
  ${VERSION_CC}
)

# Remove the temporary generated file
file(REMOVE ${VERSION_CC}.tmp)
