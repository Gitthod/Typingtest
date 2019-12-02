#include "argParse.h"
#include <memory.h>
#include <raw_term.h>
#include <speed_test.h>
#include <speed_test_sqlite.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_TEST_LENGTH     100
#define IGNORE_WHITE_SPACE      1
#define DONT_IGNORE_WHITE_SPACE 0
#define IRRELEVANT              0
#define NOT_APPLICABLE          0

#define ARG0    {"--no-white-spaces","-w", "Automatically skip all the white spaces", argBinary}

static argument arguments[] = {ARG0};

int main(int argc, char** argv)
{
    /* This function was created to avoid valgrind memory leaks. */
    atexit(freeAll);
    registerArguments(arguments, sizeof(arguments));
    parserUserInput(argv, argc);

    if (argc > 3)
    {
        printf("The program needs at most two arguments, exiting...\n");
        return -1;
    }
    else if (argc == 2)
    {
        int testLength = convertInput(argv[1]);

        if (testLength < 0)
        {
            printf("The correct format is: <prog> <filename> <name_of_test> or <prog> <number> or <prog> <dir_name>\n");
            return -1;
        }

        setAttributes(testLength, IRRELEVANT, NOT_APPLICABLE, NOT_APPLICABLE);
    }
    else if (argc == 1)
    {
        setAttributes(DEFAULT_TEST_LENGTH, IRRELEVANT, NOT_APPLICABLE, NOT_APPLICABLE);
    }
    else
    {
        char *buffer =  0; //fileToBuffer(argv[1]);
//        if ( NULL == buffer)
//        {
//            printf("Can't open the files %s\r\nexiting...\n", argv[1]);
//            return -1;
//        }

        /* If the test_name starts with an underscore ignore whitespaces. */
        if (argv[2][0] == '_')
            setAttributes(DEFAULT_TEST_LENGTH, argv[2], buffer, IGNORE_WHITE_SPACE);
        else
            setAttributes(DEFAULT_TEST_LENGTH, argv[2], buffer, DONT_IGNORE_WHITE_SPACE);
    }

    init_sqlite_db();

    while (goto_Menu());

    return 0;
}
