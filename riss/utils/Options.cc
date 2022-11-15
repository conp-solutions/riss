/**************************************************************************************[Options.cc]
Copyright (c) 2008-2010, Niklas Sorensson
Copyright (c) 2015, Norbert Manthey, LGPL v2, see LICENSE

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "riss/mtl/Sort.h"
#include "riss/utils/Options.h"
#include "riss/utils/ParseUtils.h"

using namespace Riss;

bool Riss::parseOptions(int& argc, char** argv, bool strict) {
    bool returnValue = false;
    int i, j;
    for (i = j = 1; i < argc; i++) {
        const char* str = argv[i];
        if (match(str, "--") && match(str, Option::getHelpPrefixString()) && match(str, "help")) {
            if (*str == '\0') {
                printUsageAndExit(argc, argv);
            } else if (match(str, "-verb")) {
                printUsageAndExit(argc, argv, true);
            }
            returnValue = true;
            argv[j++] = argv[i]; // keep the help parameter
        } else {
            bool parsed_ok = false;

            for (int k = 0; !parsed_ok && k < Option::getOptionList().size(); k++) {
                parsed_ok = Option::getOptionList()[k]->parse(argv[i]);

                // fprintf(stderr, "checking %d: %s against flag <%s> (%s)\n", i, argv[i], Option::getOptionList()[k]->name, parsed_ok ? "ok" :
                // "skip");
            }

            if (!parsed_ok) {
                if (strict && match(argv[i], "-")) {
                    fprintf(stderr, "ERROR! Unknown flag \"%s\". Use '--%shelp' for help.\n", argv[i], Option::getHelpPrefixString());
                    exit(1);
                }
                argv[j++] = argv[i];
            }
        }
    }

    argc -= (i - j);
    return returnValue;
}

void Riss::setUsageHelp(const char* str) {
    Option::getUsageString() = str;
}
void Riss::setHelpPrefixStr(const char* str) {
    Option::getHelpPrefixString() = str;
}
void Riss::printUsageAndExit(int argc, char** argv, bool verbose, int activeLevel) {
    const char* usage = Option::getUsageString();

    sort(Option::getOptionList(), Option::OptionLt());

    const char* prev_cat = nullptr;
    const char* prev_type = nullptr;

    for (int i = 0; i < Option::getOptionList().size(); i++) {

        if (activeLevel >= 0 && Option::getOptionList()[i]->getDependencyLevel() > activeLevel) {
            continue;
        } // can jump over full categories

        const char* cat = Option::getOptionList()[i]->category;
        const char* type = Option::getOptionList()[i]->type_name;

        if (cat != prev_cat) {
            fprintf(stderr, "\n%s OPTIONS:\n\n", cat);
        } else if (type != prev_type) {
            fprintf(stderr, "\n");
        }

        Option::getOptionList()[i]->help(verbose);

        prev_cat = Option::getOptionList()[i]->category;
        prev_type = Option::getOptionList()[i]->type_name;
    }

    if (usage != nullptr) {
        fprintf(stderr, "\n");
        fprintf(stderr, usage, argv[0]);
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "\nHELP OPTIONS:\n\n");
    fprintf(stderr, "  --%shelp        Print help message.\n", Option::getHelpPrefixString());
    fprintf(stderr, "  --%shelp-verb   Print verbose help message.\n", Option::getHelpPrefixString());
    fprintf(stderr, "\n");
    // exit(0);
}

void Riss::printOptions(FILE* pcsFile, int printLevel, int granularity) {
    sort(Option::getOptionList(), Option::OptionLt());

    const char* prev_cat = nullptr;
    const char* prev_type = nullptr;

    // all options in the global list
    for (int i = 0; i < Option::getOptionList().size(); i++) {

        if (printLevel >= 0 && Option::getOptionList()[i]->getDependencyLevel() > printLevel) {
            continue;
        } // can jump over full categories

        const char* cat = Option::getOptionList()[i]->category;
        const char* type = Option::getOptionList()[i]->type_name;

        // print new category
        if (cat != prev_cat) {
            fprintf(pcsFile, "\n#\n#%s OPTIONS:\n#\n", cat);
        } else if (type != prev_type) {
            fprintf(pcsFile, "\n");
        }

        // print the actual option
        Option::getOptionList()[i]->printOptions(pcsFile, printLevel, granularity);

        // set prev values, so that print is nicer
        prev_cat = Option::getOptionList()[i]->category;
        prev_type = Option::getOptionList()[i]->type_name;
    }
}

void Riss::printOptionsDependencies(FILE* pcsFile, int printLevel, int granularity) {
    sort(Option::getOptionList(), Option::OptionLt());

    const char* prev_cat = nullptr;
    const char* prev_type = nullptr;

    // all options in the global list
    for (int i = 0; i < Option::getOptionList().size(); i++) {

        if (Option::getOptionList()[i]->dependOnNonDefaultOf == 0 ||                              // no dependency
            (printLevel >= 0 && Option::getOptionList()[i]->getDependencyLevel() > printLevel)) { // or too deep in the dependency level
            continue;
        } // can jump over full categories

        const char* cat = Option::getOptionList()[i]->category;
        const char* type = Option::getOptionList()[i]->type_name;

        // print new category
        if (cat != prev_cat) {
            fprintf(pcsFile, "\n#\n#%s OPTIONS:\n#\n", cat);
        } else if (type != prev_type) {
            fprintf(pcsFile, "\n");
        }

        // print the actual option
        Option::getOptionList()[i]->printOptionsDependencies(pcsFile, printLevel, granularity);

        // set prev values, so that print is nicer
        prev_cat = Option::getOptionList()[i]->category;
        prev_type = Option::getOptionList()[i]->type_name;
    }
}

void Riss::configCall(int argc, char** argv, std::stringstream& s) {
    // print all options that are left over
    int i, j;
    for (i = j = 1; i < argc; i++) {
        const char* str = argv[i];
        s << argv[i] << " ";
    }

    // fill the stream for all the options
    for (int i = 0; i < Option::getOptionList().size(); i++) {
        // skip the option "-cmd", because this option is responsible to print the command line
        if (Option::getOptionList()[i]->name != 0 && strcmp(Option::getOptionList()[i]->name, "cmd") == 0) {
            continue;
        }
        // if there is an option that has not its default value, print its call
        if (!Option::getOptionList()[i]->hasDefaultValue()) {
            Option::getOptionList()[i]->printOptionCall(s);
            s << " ";
        }
    }
}
