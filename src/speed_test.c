#include <memory.h>
#include <raw_term.h>
#include <speed_test.h>
#include <speed_test_sqlite.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>


#define CTRL_KEY(k) ((k) & 0x1f)

/* Static variables */
static char *Global_Ar[] = {"qw",
    "as",
    "zx",
    "op",
    "l;",
    "./"};


/* Test length for the 2 fingers test. */
static int G_Test_Length;
/* Will point to the converted file to characters */
static char *buffer = NULL;
/* Entry name for the sqlite db. */
static char *test_name = NULL;
/* Custom struct to store attributes of the current terminal session. */
static termAttributes *sh_Attrs;
/* Ignore all white spaces expept newLines. */
static char skipWhiteSpace = 0;

/* Function declarations */

/*
 * Checks whether the character c belongs to Global_Ar, in that case the function returns 1 , *idx contains the
 * index of char in the static array, and ptr is used to change the value of a char * to show the beginning of that
 * string.
 */
static int checkValidKey(char c, char *idx, char** ptr);

/*
 * Returns a string containing the possible values for id. This is used to filter the availabe keys for the different
 * menus (id is the menu's name).
 */
static char *getListFromId(char *id);

/*
 * Uses getchar but converts the char to lower and returns it.
 */
static int l_getchar(void);

/*
 * Check whether char c belongs to the list returned from getListFromId(id).
 */
static int notValidChar(char c, char *id);

/*
 * Menu to perform some sql queries (get best and average times) in the sqlite db.
 */
static void browse_DB(void);

/*
 * test points to the beginning of a converted file into characters, test_name will be used to store the result in
 * the database.
 * This function performs this custom typing test.
 */
static void custom_test(char *test, char *test_name);

/*
 * Performs the default 2finger test.
 */
static void typingTest(void);


static void typingTest(void)
{
    char c;
    /* Ptr to the character in character tuple to be tested */
    char *ptr;
    /* Index of the character (0 or 1) */
    char idx;
    struct timeval end, start, result;
    int repeat;
    char *message = 0;
    char *test_message = 0;
    float test_time;
    float cpm = 0;

    asprintf(&test_message, "Type as fast as you can %u letters:\n", G_Test_Length);
    forCleanup(test_message);
    dumpRows(test_message, 0, sh_Attrs->numrows);
    int test_offset = sh_Attrs->numrows;

    enableCursor();

    do
    {
        /* The number of mistakes */
        int mistakes = 0;
        repeat = 0;
START:
        setAppMessage("\x1b[37mWhen you start typing this will line will show your CPM");
        delRows(test_offset);
        while ((c = l_getchar()))
        {
            if (checkValidKey(c, &idx, &ptr))
            {
                insertChar(c);

                gettimeofday(&start, NULL);
                break;
            }

            if (CTRL_KEY('b') == c)
            {
                delRows(test_offset);
                dumpRows("Exiting test...", 0, sh_Attrs->numrows);
                sleep(1);
                return;
            }
        }

        for (int i = 1; i < G_Test_Length + 1; i++)
        {
            int colorCode = 0;

            gettimeofday(&end, NULL);
            timersub(&end, &start, &result);
            test_time = result.tv_sec + (float)result.tv_usec / 1000000;
            cpm = i / test_time * 60;

            if (cpm <= 200.0)
                /* RED */
                colorCode = 31;
            else if (cpm <= 300.0)
                /* YELLOW */
                colorCode = 33;
            else if (cpm <= 400.0)
                /* GREEN */
                colorCode = 32;
            else if (cpm <= 500.0)
                /* BLUE */
                colorCode = 34;
            else if (cpm <= 600)
                /* MAGENTA */
                colorCode = 35;
            else if (cpm <= 700)
                /* CYAN */
                colorCode = 36;
            else
                /* BRIGHT RED */
                colorCode = 91;


            setAppMessage("\x1b[%dmYour current CPM is : %.2f | \x1b[37mMistakes(Accuracy) : %d(%.2f\%)", colorCode, cpm,
                    mistakes, (float)mistakes / i * 100);

            /* Case isn't important for this test. */
            c = l_getchar();
            if (c != ptr[!idx]) {
                if (CTRL('r') == c)
                {
                    delRows(test_offset);
                    dumpRows("Resetting...", 0, sh_Attrs->numrows);
                    mistakes = 0;
                    sleep(1);
                    goto START;
                }
                else if (CTRL('b') == c)
                {
                    delRows(test_offset);
                    dumpRows("Exiting test...", 0, sh_Attrs->numrows);
                    sleep(1);
                    return;
                }
                i--;
                mistakes++;
            }
            else
            {
                idx = !idx;
                insertChar(c);
            }
        }

        asprintf(&message, "Your CPM was %.2f", cpm);
        dumpRows(message, 0, sh_Attrs->numrows);
        free(message);

        insert(ptr, G_Test_Length, mistakes, result.tv_sec + (float)result.tv_usec / 1000000);
        asprintf(&message, "\nYou finished in %lu.%06lu seconds \nYou made %d mistakes\nRepeat the test? (y\\n)\n",
                result.tv_sec, result.tv_usec, mistakes);
        dumpRows(message, 0, sh_Attrs->numrows);
        free(message);

        while ((c = l_getchar()) != 'y' && c != 'n');

        if ('y' == c)
            repeat = 1;
        else
            repeat = 0;

    } while (repeat);
}

/* Make it work with scrolling maybe calculate the availabe screen and split it in two
   or work with the highlighted word. */
static void custom_test(char *test, char *test_name)
{
    char c;
    struct timeval end, start, result;
    int repeat;

    char *test_message = 0;
    asprintf(&test_message, "The test is :\n%s", test);

    int size_read = 0;
    int reset_size;
    size_read = dumpRows(test_message, 4, sh_Attrs->numrows);
    forCleanup(test_message);
    reset_size = size_read;

    dumpRows("\n**************************************************\n\n", 0, sh_Attrs->numrows);
    enableCursor();
    int test_offset = sh_Attrs->numrows;

    do
    {
        /* The number of mistakes */
        int whiteSpace = 0;
        int mistakes = 0;
        int idx = 0;
        float test_time = 0;
        float cpm = 0;
        repeat = 0;
START:
        idx = 0;
        delRows(test_offset - 7);
        size_read = reset_size;
        setAppMessage("\x1b[37mWhen you start typing this will line will show your CPM");
        dumpRows(test_message, 4, sh_Attrs->numrows);
        dumpRows("\n**************************************************\n\n", 0, sh_Attrs->numrows);

        mistakes = 0;
        whiteSpace = 0;

        while ((c = getKey()) != test[idx])
        {
            /* The enter key which represents a new line is \r, therefore this check has to be done. */
            if (test[idx] == '\n' && c == '\r')
                break;

            if (CTRL_KEY('b') == c)
            {
                delRows(test_offset);
                dumpRows("Exiting test...", 0, sh_Attrs->numrows);
                sleep(1);
                return;
            }
        }

        /* Don't try to insert a new line character. */
        if (c != '\r')
            insertChar(c);
        else
        {
            /* Advance the text by one line if c == '\r' . */
            delRow(test_offset - 6);
            size_read += dumpRows(test_message + size_read, 1, test_offset - 4);
            delRows(test_offset);
        }

        idx++;
        /* Skip all the connected white spaces. */
        if (c == ' ' || c == '\t')
            while (test[idx] == ' ' || test[idx] == '\t')
            {
                whiteSpace++;
                insertChar(test[idx++]);
            }

        gettimeofday(&start, NULL);

        while (test[idx])
        {
            int colorCode = 0;

            if (skipWhiteSpace)
                if (test[idx] == ' '  ||
                    test[idx] == '\t' ||
                    test[idx] == '\n'
                   )
                {
                    if (test[idx] == '\n')
                    {
                        delRow(test_offset - 6);
                        size_read += dumpRows(test_message + size_read, 1, test_offset - 4);
                        delRows(test_offset);
                        idx++;
                    }
                    else
                    {
                        whiteSpace++;
                        insertChar(test[idx++]);
                    }
                    continue;
                }

            gettimeofday(&end, NULL);
            timersub(&end, &start, &result);
            test_time = result.tv_sec + (float)result.tv_usec / 1000000;
            cpm = (idx - whiteSpace)/ test_time * 60;

            if (cpm <= 200.0)
                /* RED */
                colorCode = 31;
            else if (cpm <= 300.0)
                /* YELLOW */
                colorCode = 33;
            else if (cpm <= 400.0)
                /* GREEN */
                colorCode = 32;
            else if (cpm <= 500.0)
                /* BLUE */
                colorCode = 34;
            else if (cpm <= 600)
                /* MAGENTA */
                colorCode = 35;
            else if (cpm <= 700)
                /* CYAN */
                colorCode = 36;
            else
                /* BRIGHT RED */
                colorCode = 91;

            setAppMessage("\x1b[%dmYour current CPM is : %.2f | \x1b[37mMistakes(Accuracy) : %d(%.2f\%)", colorCode, cpm,
                    mistakes, (float)mistakes / idx * 100);
            c = getKey();
            if (c != test[idx] && (c != '\r' || test[idx] != '\n'))
            {
                mistakes++;

                if (CTRL_KEY('r') == c)
                {
                    delRows(test_offset);
                    dumpRows("Resetting...", 0, sh_Attrs->numrows);
                    sleep(1);
                    goto START;
                }
                else if (CTRL_KEY('b') == c)
                {
                    delRows(test_offset);
                    dumpRows("Exiting test...", 0, sh_Attrs->numrows);
                    sleep(1);
                    return;
                }
            }
            else
            {
                ++idx;
                /* The if condition ensures that test[idx] == '\n' . */
                if (c == '\r')
                {
                    delRow(test_offset - 6);
                    size_read += dumpRows(test_message + size_read, 1, test_offset - 4);
                    delRows(test_offset);
                }
                else
                {
                    int temp = sh_Attrs->cy;
                    insertChar(c);
                    colorRow(7, sh_Attrs->cx, GREEN);

                    /* Check if we reached a new line by passing the line limit. */
                    if (sh_Attrs->cy != temp)
                    {
                        delRow(sh_Attrs->numrows - 2);
                        delRow(test_offset - 6);
                        size_read += dumpRows(test_message + size_read, 1, test_offset - 4);
                    }

                    /* Treat consecutive white spaces as one. */
                    if (c == ' ' || c == '\t')
                        while (test[idx] == ' ' || test[idx] == '\t')
                        {
                            whiteSpace++;
                            insertChar(test[idx++]);
                        }
                }
            }
        }

        int test_length = strlen(test);
        insert(test_name, test_length, mistakes, test_time);

        char *message;
        asprintf(&message, "Your CPM was %.2f", cpm);
        dumpRows(message, 0, sh_Attrs->numrows);
        free(message);

        asprintf(&message, "\nYou finished in %lu.%06lu seconds \nYou made %d mistakes\nRepeat the test? (y\\n)\n",
                result.tv_sec, result.tv_usec, mistakes);
        dumpRows(message, 0, sh_Attrs->numrows);
        free(message);

        while ((c = l_getchar()) != 'y' && c != 'n');

        if ('y' == c)
            repeat = 1;
        else
            repeat = 0;

    } while (repeat);
}

static int checkValidKey(char c, char *idx, char** ptr)
{
    for (int i = 0; i < sizeof(Global_Ar) / sizeof(Global_Ar[0]); i++)
        for (int j = 0; j < 2; j++)
            if (c == Global_Ar[i][j])
            {
                *idx = j;
                *ptr = Global_Ar[i];
                return 1;
            }

    return 0;
}

int convertInput(char* input)
{
    int result = 0;
    while (*input)
    {
        if (*input >= 48 && *input <= 57)
        {
            result = 10 * result + *input++ - 48;
        }
        else
            return -1;
    }
    return result;
}

static int l_getchar(void)
{
    int c = getKey();
    if ( c >= 65 && c <= 90)
        c += 32;

    return c;
}

int goto_Menu(void)
{
    char c;
    static int init = 0;
    static int test_offset = 0;


    char *Menu = "##################################################\n"
        "Type enter or tab to enter the auto test.\n"
        "Type c to enter the custom test.\n"
        "Type b to browse the database.\n"
        "Type q to quit the applications.\n"
        "##################################################\n";

    disableCursor();
    if (init == 0)
    {
        enableRawMode();
        sh_Attrs = initShellAttributes();
        dumpRows(Menu, 0, sh_Attrs->numrows);
        init = 1;
        test_offset = sh_Attrs->numrows;
    }
    else
    {
        delRows(test_offset);
    }
    setAppMessage(">>>>>>>>>>>>>>>MAIN MENU<<<<<<<<<<<<<<<");

    while (notValidChar(c = getKey(), "Menu"));

    switch(c)
    {
        case 'c':
            if (NULL == buffer)
                pexit("No custom test was given\n");
            custom_test(buffer, test_name);
            return 1;
        case '\r':
            typingTest();
            return 1;
        case '\t':
            typingTest();
            return 1;
        case 'q':
            return 0;
        case 'b':
            browse_DB();
            return 1;
        default :
            return 1;
    }

}

static void browse_DB(void)
{
    int c;
    int stay = 1;
    char *menu = "Type 1 to fetch the fastest times\n"
        "Type 2 to browse average times\n"
        "Type 3 to get statistics for all tests\n"
        "Type x to exit the DB menu\n"
        "##################################################\n";

    char test_name[20];
    dumpRows(menu, 0, sh_Attrs->numrows);
    setAppMessage(">>>>>>>>>>>>>>>>DB MENU<<<<<<<<<<<<<<<<");
    int menu_end = sh_Attrs->numrows;

    while (stay)
    {
        disableCursor();
        while (notValidChar(c = l_getchar(), "DB"));
        enableCursor();

        int cnt = 0;
        switch(c)
        {
            case '1' :
                dumpRows("Which test to browse?\n", 0, sh_Attrs->numrows);
                while ((c = getKey()) != '\r' && cnt < 20)
                {
                    insertChar(c);
                    test_name[cnt++] = c;
                }

                test_name[cnt] = 0;
                dumpRows("How many results to fetch (0-9)?\n", 0, sh_Attrs->numrows);

                while ((c = getKey()) < 48 && c > 58);
                insertChar(c);

                if (get_nmin(test_name, G_Test_Length, c ))
                    init_sqlite_db();

                dumpRows("Press Enter to return", 0, sh_Attrs->numrows);
                while ((c = getKey()) != '\r')
                    moveCursor(c);
                break;

            case '2' :
                dumpRows("Which test to browse?\n", 0, sh_Attrs->numrows);

                while ((c = getKey()) != '\r' && cnt < 20)
                {
                    insertChar(c);
                    test_name[cnt++] = c;
                }

                test_name[cnt] = 0;

                dumpRows("Browse all test averages? (y/n)\n", 0, sh_Attrs->numrows);

                while ((c = l_getchar()) != 'y' && c != 'n');

                if ('n' == c)
                {
                    if (get_average(test_name, G_Test_Length))
                        init_sqlite_db();
                }
                else
                {
                    if (get_all_averages(test_name))
                        init_sqlite_db();
                }

                dumpRows("Press Enter to return", 0, sh_Attrs->numrows);
                while ((c = getKey()) != '\r')
                    moveCursor(c);
                break;

            case '3':
                if (get_all_averages(0))
                    init_sqlite_db();

                dumpRows("Press Enter to return", 0, sh_Attrs->numrows);
                while ((c = getKey()) != '\r')
                    moveCursor(c);
                break;

            case 'x' :
                stay = 0;
                break;
        }

        delRows(menu_end);
    }

}

static int notValidChar(char c, char *id)
{
    char *validChars;
    int idx = 0;
    validChars = getListFromId(id);

    while (validChars[idx])
    {
        if ((c == validChars[idx++]))
            return 0;
    }

    return 1;
}

static char *getListFromId(char *id)
{
    static char *list[][2] ={{"DB","123x"},
        {"Menu","\t\rqbc"} };

    for (int i = 0; i < sizeof(list) / sizeof(list[0]); i++)
    {
        if (strcmp(list[i][0], id) == 0)
            return list[i][1];
    }

    return 0;
}


char *fileToBuffer(char *filename)
{
    FILE *f = fopen(filename, "rb");

    if (f == NULL)
    {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    /* Editors are likely to add an extra \n character (0xa) in the end of the file. */
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *string = (char *)malloc(fsize + 1);
    fread(string, fsize, 1, f);
    string[fsize] = 0;
    fclose(f);

    return string;
}

void setAttributes(int testLength, char *testName, char *fileBuffer, char ignoreWhiteSpace)
{
    if (testLength)
        G_Test_Length = testLength;

    if (ignoreWhiteSpace)
        skipWhiteSpace = 1;

    if (testName)
        test_name = testName;

    if (fileBuffer)
    {
        buffer = fileBuffer;
        forCleanup(buffer);
    }

}
