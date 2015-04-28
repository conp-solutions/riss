# Project definitions:
#
project(scripts)

set(SCRIPTS
    Riss427.sh
    Riss427DRAT.sh
    cp.sh
    dratChecked.sh
    findMinimumBound.sh
    pcasso.sh
    publish.sh
    qp.sh
    shiftBMC-dumpCNF.sh)

add_custom_target(scripts)

# copy all scripts to the binary directory if their content
# has changed
foreach(script ${SCRIPTS})
    add_custom_command(TARGET scripts POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                ${CMAKE_CURRENT_SOURCE_DIR}/${script}
                                                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endforeach(script)
