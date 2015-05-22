import re
from pathlib import Path

# replace_std  = re.compile(r'(?<!std::)(cerr|cout|endl|(?<!\<)vector|string|ostream)')
using_namespace = re.compile(r'^using namespace')

std_types = [
    # iostream
    r'(?<!std::)(?<!\w)(cerr)',
    r'(?<!std::)(?<!\w)(cout)',
    r'(?<!std::)(?<!\w)(endl)',
    r'(?<!std::)(?<!\w)(ostream)',
    r'(?<!std::)(?<!\w)(string)',
    r'(?<!std::)(?<!\w)(vector)(?!\w)',
    r'(?<!std::)(?<!\w)(deque)',
    r'(?<!std::)(?<!\w)(pair)',
]

riss_types = [
    r'(?<!Riss::)(?<!\w)(ClauseAllocator)',
    r'(?<!Riss::)(?<!\w)(ThreadController)',
    r'(?<!Riss::)(?<!\w)(CRef)',
    r'(?<!Riss::)(?<!\w)(Solver)',
    r'(?<!Riss::)(?<!\w)(MarkArray)',
    r'(?<!Riss::)(?<!\w)(Clause)(?!\w)',
    r'(?<!Riss::)(?<!\w)(Heap)(?!\w)',
    r'(?<!Riss::)(?<!std::)(vec)(?!tor)(?=[\w <])',
    r'(?<!Riss::)(?<!\w)(Lit)(?!\w)',
    r'(?<!Riss::)(?<!\w)(Var)(?!\w)',

    # Option types
    r'(?<!Riss::)(?<!\w)(IntOption)',
    r'(?<!Riss::)(?<!\w)(DoubleOption)',
    r'(?<!Riss::)(?<!\w)(StringOption)',
    r'(?<!Riss::)(?<!\w)(IntRange)',

    # lifted boolean
    r'(?<!Riss::)(?<!\w)(lbool)(?!\w)',
    # this are #define's noe
    # r'(?<!Riss::)(?<!\w)(lit_Undef)(?!\w)',
    # r'(?<!Riss::)(?<!\w)(lit_False)(?!\w)',
    # r'(?<!Riss::)(?<!\w)(lit_True)(?!\w)',

    # Functions
    r'(?<!Riss::)(?<!\w)(toLit)(?!\w)',
    r'(?<!Riss::)(?<!\w)(mkLit)(?!\w)',
]

pcasso_types = [
    r'(?<!Pcasso::)(?<!\w)(SplitterSolver)(?!\w)',
]

# compile all expressions
riss_types = [re.compile(t) for t in riss_types]
std_types  = [re.compile(t) for t in std_types]

replace_double_riss = re.compile(r'Riss::Riss::')

folders = [
    Path('classifier'),
    Path('coprocessor'),
    Path('mprocessor'),
    Path('pcasso'),
    Path('pfolio'),
    Path('proofcheck'),
    Path('qprocessor'),
    Path('riss'),
    
    # Path('risslibcheck'),
    # Path('aiger'),
    # Path('shiftbmc'),

    # Path('scripts'),
    # Path('test'),
    # Path('tools'),
    # Path('doc'),
    # Path('cmake'),
]

# riss_path = Path('riss')

for folder in folders:
    for path in folder.glob('**/*.h'):
        lines = []

        with path.open('r') as in_file:
            for line in in_file:

                if not line.startswith('#include'):
                    # line = re.sub(using_namespace, r'// using namespace', line)
                    # line = re.sub(replace_std, r'std::\1', line)

                    for p in std_types:
                        line = re.sub(p, r'std::\1', line)

                    # for p in riss_types:
                    #     line = re.sub(p, r'Riss::\1', line)

                    # for p in pcasso_types:
                    #     line = re.sub(p, r'Pcasso::\1', line)

                    line = re.sub(r'Riss::(l_Undef|l_True|l_False)', r'\1', line)

                    # line = re.sub(replace_riss, r'Riss::\1', line)
                    line = re.sub(replace_double_riss, r'Riss::', line)

                lines.append(line)

        # if path.parent != riss_path:
        #     for i, line in enumerate(lines):
        #         lines[i] = re.sub(replace_riss, r'Riss::\1', line)

        with path.open('w') as out_file:
            for line in lines:
                out_file.write(line)

