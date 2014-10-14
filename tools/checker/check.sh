./minisat ~/cnf/sudoku76.cnf /tmp/minisat-out -distribute=1 > /dev/null 2> /dev/null
status=$?
cat /tmp/minisat-out
exit $status
