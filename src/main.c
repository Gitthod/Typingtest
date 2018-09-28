/* Some header can be included in others, however i will include them again to show the dependency. */
#define _GNU_SOURCE // This is only defined for asprintf

#include <speed_test.h>
#include <raw_term.h>
#include <speed_test_sqlite.h>
#include <stdio.h>     // asprintf needs _GNU_SOURCE

#define DEFAULT_TEST_LENGTH 100

int main(int argc, char** argv)
{
    /* This function was created to avoid valgrind memory leaks. */
    atexit(freeAll);

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
            printf("The correct format is: <prog> <filename> <name_of_test> or <prog> <number>\n");
            return -1;
        }

        setAttributes(testLength, 0, 0);
    }
    else if (argc == 1)
    {
        setAttributes(DEFAULT_TEST_LENGTH, 0, 0);
    }
    else
    {
        char *buffer =  fileToBuffer(argv[1]);
        if ( NULL == buffer)
        {
            printf("Can't open the filse %s\r\nexiting...\n", argv[1]);
            return -1;
        }
        setAttributes(DEFAULT_TEST_LENGTH, argv[2], buffer);
    }

    init_sqlite_db();

    while (goto_Menu());

    return 0;
}
