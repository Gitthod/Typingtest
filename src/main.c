#include "argParse.h"
#include <memory.h>
#include <raw_term.h>
#include <speed_test.h>
#include <speed_test_sqlite.h>
#include <stdio.h>
#include <stdlib.h>
#include "speed_browser.h"

#define DEFAULT_TEST_LENGTH     100
#define IGNORE_WHITE_SPACE      1
#define DONT_IGNORE_WHITE_SPACE 0
#define IRRELEVANT              0
#define NOT_APPLICABLE          0

#define ARG0    {"--no-white-spaces","-w", "Automatically skip all the white spaces", argBinary}
#define ARG1    {"--test-length", "-l", "Set the test length for the automatic tests", argVariable, "71"}
#define ARG2    {"--test-directory", "-d", "Set the test length for the automatic tests", argVariable, "./tests"}

static argument arguments[] = {ARG0, ARG1, ARG2};

int main(int argc, char** argv)
{
    /* This function was created to avoid valgrind memory leaks. */
    atexit(freeAll);
    registerArguments("typing test",arguments, sizeof(arguments), 0, 0);
    parserUserInput(argv, argc);

    if (getArgValue("-w") == NOT_NULL_ARG)
        setAttributes(DEFAULT_TEST_LENGTH, argv[2], NOT_APPLICABLE, IGNORE_WHITE_SPACE);
    else
        setAttributes(DEFAULT_TEST_LENGTH, argv[2], NOT_APPLICABLE, DONT_IGNORE_WHITE_SPACE);

    int testLength = convertInput(getArgValue("-l"));
    setAttributes(testLength, IRRELEVANT, NOT_APPLICABLE, NOT_APPLICABLE);
    changeTestDir(getArgValue("-d"));
    freeArgs();
    init_sqlite_db();

    while (goto_Menu());

    return 0;
}
