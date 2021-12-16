#!/bin/bash
# coprocessor-for-modelcounting.sh, Norbert Manthey, 2020
#
# Simplify a given formula, and solve it with the given solver

SCRIPT_DIR="$(dirname $0)"

# Global parameters
INPUT=""
COPROCESSOR_EXTRA_ARGS=""
COPROCESSOR="$SCRIPT_DIR/../build/bin/coprocessor"
OUTPUT_LOCATION=""
DEBUG=""
AWK="awk"
COPROCESSOR_FAILBACK="false"

usage () 
{
cat << EOF

  $0 [options] <input> [<solver> [<solver-args>]]
  Norbert Manthey, 2020

  This script will simplify a given CNF into another CNF that has the same
  number of models, and next call the given solver to solve it

  -a AWK ........... (GNU) awk tool to be used, (default: $AWK)
  -c coprocessor ... location of coprocessor (default: $COPROCESSOR)
  -F ............... fail back to no simplification in case coprocessor fails
  -p CLI ........... add this CLI parameters to coprocessor
  -o FILE .......... write simplified file to this location, instead of solving
  
  To get more insights into this scripts internals
  -d ............... Print some debugging output
  -t ............... Enable tracing, which will print each command that is run
  -e ............... Fail on error, i.e. in case a run command fails. Using this
                     parameter allows to detect errors in the script, which is
                     useful in case it is used in automation.
EOF
}

# Handle CLI
while getopts "ac:deFho:p:t" o; do
    case "${o}" in
        a)
            AWK="${OPTARG}"
            ;;
        c)
            COPROCESSOR="${OPTARG}"
            ;;
        d)
            DEBUG="true"
            ;;
        F)
            COPROCESSOR_FAILBACK="true"
            ;;
        e)  set -e
            ;;
        h)
            usage
            exit 0
            ;;    
        p)
            COPROCESSOR_EXTRA_ARGS="${OPTARG}"
            ;;
        o)
            OUTPUT_LOCATION="${OPTARG}"
            ;;
        t)  set -x
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done
shift $((OPTIND-1))

INPUT="$1"
shift

# Check for Coprocessor
if ! command -v "$COPROCESSOR" &> /dev/null; then
    echo "Error: cannot find coprocessor at $COPROCESSOR, abort"
    usage
    exit 1
fi

# Check for given file
if [ ! -r "$INPUT" ]
then
    echo "Error: cannot read input formula $INPUT, abort"
    usage
    exit 1
fi

# Check tools we need, and abort
for tool in $AWK grep zgrep basename cat zcat
do
    FAILURE=0
    if ! command -v "$COPROCESSOR" &> /dev/null; then
        echo "Error: cannot find tool: $tool"
        FAILURE=1
    fi
    [ "$FAILURE" -gt 0 ] && exit 1
done

# Create a temporary work directory, make sure we clean up in the end
[ -n "$DEBUG" ] && trap '[ -d "$WORK_DIR" ] && rm -rf $WORK_DIR' EXIT
WORK_DIR=$(mktemp -d)
[ -n "$DEBUG" ] && echo "debug: will work in directory $WORK_DIR"


# make sure coprocessor does not change equivalence
WEIGHT_FREE_CNF="$WORK_DIR/weight-free-$(basename $INPUT .gz)"
touch "$WEIGHT_FREE_CNF"
if [ ! -r "$WEIGHT_FREE_CNF" ]; then
    echo "Could not create temporary file '$WEIGHT_FREE_CNF'"
    exit 1
fi

# count number of vars in file
declare -i VARS
WEIGHT_LINES=""
COMMENTS=""
P_LINE=""
if [[ "$INPUT" = *.gz ]]; then
    VARS="$(zcat "$INPUT" | $AWK '/^p / {print $4}')"
    WEIGHT_LINES=$(zgrep "^w" "$INPUT")
    COMMENTS=$(zgrep "^c" "$INPUT")
    P_LINE=$(zgrep "^p" "$INPUT")
    zgrep -v "^w" "$INPUT" > "$WEIGHT_FREE_CNF"
else
    VARS="$(cat "$INPUT" | $AWK '/^p / {print $4}')"
    WEIGHT_LINES=$(grep "^w" "$INPUT")
    COMMENTS=$(grep "^c" "$INPUT")
    P_LINE=$(grep "^p" "$INPUT")
    grep -v "^w" "$INPUT" > "$WEIGHT_FREE_CNF"
fi

# In case the initial formula said 'p wcnf', drop the 'w' for Coprocessor
sed -i 's:p wcnf:p cnf:g' "$WEIGHT_FREE_CNF"

# make sure coprocessor does not change equivalence #TODO: could be changed to allow eliminating variables
WHITE_FILE="$WORK_DIR/white.var"
touch "$WHITE_FILE"
if [ ! -r "$WHITE_FILE" ]; then
    echo "Could not create temporary file '$WHITE_FILE'"
    exit 1
fi
echo "1..$VARS" > "$WHITE_FILE"

# make sure coprocessor does not change equivalence
SIMPLIFIED_CNF="$WORK_DIR/simplified-$(basename $INPUT .gz)"
touch "$SIMPLIFIED_CNF"
if [ ! -r "$SIMPLIFIED_CNF" ]; then
    echo "Could not create temporary file '$SIMPLIFIED_CNF'"
    exit 1
fi

# we want the CP3 output
CP3_STDERR="$WORK_DIR/cp3_stderr.cnf"
touch "$CP3_STDERR"
if [ ! -r "$CP3_STDERR" ]; then
    echo "Could not create coprocessor stderr file '$CP3_STDERR'"
    exit 1
fi

# we want an intermediate output formula
TMP_OUTPUT="$WORK_DIR/output.cnf"
touch "$TMP_OUTPUT"
if [ ! -r "$TMP_OUTPUT" ]; then
    echo "Could not create temporary output file '$TMP_OUTPUT'"
    exit 1
fi


# Simplify formula to an equivalent one
echo "c start simplification ..."
declare -i SIMPLIFY_STATUS=0
"$COPROCESSOR" "$WEIGHT_FREE_CNF" \
    $COPROCESSOR_EXTRA_ARGS \
    -whiteList="$WHITE_FILE" \
    -dimacs="$SIMPLIFIED_CNF" \
    -no-dense \
    -search=0 \
    2> "$CP3_STDERR" \
    || SIMPLIFY_STATUS=$?
echo "c simplficitaion returned with $SIMPLIFY_STATUS"

if [ "$SIMPLIFY_STATUS" -ne 0 ] && [ "$SIMPLIFY_STATUS" -ne 20 ] && [ "$SIMPLIFY_STATUS" -ne 10 ]; then
    echo "c exit, due to simplification status $SIMPLIFY_STATUS"
    echo "=== BEGIN COPROCESSOR STDERR ==="
    cat "$CP3_STDERR"
    echo "===  END  COPROCESSOR STDERR ==="
    if [ "$COPROCESSOR_FAILBACK" != "true" ]
    then
        exit $SIMPLIFY_STATUS
    else
        # skip faulty simplification
        cp "$WEIGHT_FREE_CNF" "$SIMPLIFIED_CNF" 
        echo "c recover failed simplification by skipping over it"
        SIMPLIFY_STATUS=0
    fi
fi

# In case the formula is unsat, fake the empty formula to work around a
# sharpSAT bug that cannot handle empty clauses
if [ "$SIMPLIFY_STATUS" -eq 20 ]
then
    cat << EOF > "$SIMPLIFIED_CNF"
p cnf 2 4
1 2 0
-1 2 0
1 -2 0
-1 -2 0
EOF

fi

# Assemble the new output formula
echo "c simplified, model count equivalent, formula" > "$TMP_OUTPUT"
echo "$P_LINE" >> "$TMP_OUTPUT"
[ -n "$COMMENTS" ] && echo "$COMMENTS" >> "$TMP_OUTPUT"
[ -n "$WEIGHT_LINES" ] && echo "$WEIGHT_LINES" >> "$TMP_OUTPUT"
[ -n "$DEBUG" ] && cat "$CP3_STDERR" >> "$TMP_OUTPUT"
grep -v "^p " "$SIMPLIFIED_CNF" | grep -v "^c" >> "$TMP_OUTPUT"

# If there is an output location, copy and stop
if [ -n "$OUTPUT_LOCATION" ]; then
    echo "c copying formula to $OUTPUT_LOCATION"
    if [[ "$OUTPUT_LOCATION" = *.gz ]]; then
        OUTPUT_LOCATION="$(basename "$OUTPUT_LOCATION" .gz)"
        cp "$TMP_OUTPUT" "$OUTPUT_LOCATION"
        gzip "$OUTPUT_LOCATION"
    else
        cp "$TMP_OUTPUT" "$OUTPUT_LOCATION"
    fi
    exit 0
fi

# Solve given formula, and return with it's exit code
echo "c solving simplified formula, by running $* $TMP_OUTPUT ..."
declare -i SOLVER_STATUS=0
"$@" $TMP_OUTPUT || SOLVER_STATUS=$?
exit $SOLVER_STATUS
