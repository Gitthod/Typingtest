#include "speed_browser.h"
#include "speed_test.h"
#include "unistd.h"
#include <dirent.h>
#include <memory.h>
#include <openssl/sha.h>
#include <raw_term.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------------------------------------------------------ */
/* ---------------------------------------------------- Defines ----------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

#define FILTERED_OUT       1
#define PASS               0

/* ------------------------------------------------------------------------------------------------------------------ */
/* ------------------------------------------- Internal Types Definition -------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

typedef enum cursorStatus {
    REACHED_TOP = -1,
    MOVEMENT_DONE,
    REACHED_BOTTOM
} cursorStatus;

/* ------------------------------------------------------------------------------------------------------------------ */
/* ------------------------------------------------ Static Variables ------------------------------------------------ */
/* ------------------------------------------------------------------------------------------------------------------ */

/* Custom struct to store attributes of the current terminal session. */
static termAttributes *sh_Attrs;

/* Filter out files that end with the following patterns. */
static char *bannedEndings[] = {".swp", ".swo" };

/* Filter out files that start with the following patterns. */
static char *bannedBeginnings[] = {"."};

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Static Function Declarations ---------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

/* Moves the cursor when selecting files. */
static cursorStatus moveBrowseCursor(uint32_t min, uint32_t max, uint32_t *current, int movement, termAttributes *E);

/*
 * Filter out specific endings or beginnings of files in file selection Menu.
 */
static int filterFiles(const char *fileName);

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Static Function Implementations ------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

static int filterFiles(const char *fileName)
{
    const char *begin = fileName;
    const char *end = fileName;
    const char *idx = 0;
    const char *widx = 0;

    /* Make end point to the end for fileName. */
    while(*end++);

    /* Now end points to the string terminator of fileName. */
    end--;

    for (int i = 0; i < sizeof(bannedBeginnings) / sizeof(char *); i++)
    {
        widx = bannedBeginnings[i];
        idx = begin;

        while(*widx && *idx && *widx == *idx)
            idx++, widx++;

        if (*widx == 0)
            return FILTERED_OUT;
    }

    for (int i = 0; i < sizeof(bannedEndings) / sizeof(char *); i++)
    {
        widx = bannedEndings[i];
        idx = end;

        /* Make widx point to the end of the banned ending. */
        while(*widx++);

        widx--;

        while(widx >= bannedEndings[i] && idx >= begin && *widx == *idx)
            idx--, widx--;

        if (widx == bannedEndings[i] - 1)
            return FILTERED_OUT;
    }

    return PASS;
}

static cursorStatus moveBrowseCursor(uint32_t min, uint32_t max, uint32_t *current, int movement, termAttributes *E)
{
    char *cursor = "\xe2\x86\x90"; /* Leftward arrow. */
    if (*current == max && (movement == ARROW_DOWN || movement == 'j'))
    {
        return REACHED_BOTTOM;
    }
    else if (*current == min && (movement == ARROW_UP || movement == 'k'))
    {
        return REACHED_TOP;
    }
    else
    {
        rowTruncateString(&E->row[*current], 3);

        if (movement == ARROW_DOWN || movement == 'j')
        {
            rowAppendString(&E->row[(*current)++ + 1], cursor, 3);
        }
        else
        {
            rowAppendString(&E->row[(*current)-- - 1], cursor, 3);
        }
        return MOVEMENT_DONE;
    }
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Global Function Implementations ------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

void selectTest(void)
{
    int menu_start;
    currentTest *cTest = getCurrentTest();
    char *testDir = "tests";
    char *message;

    char *menu = "Select the file containing the text you want to be tested upon\n"
                 "##############################################################\n";

    if (!sh_Attrs)
    {
        sh_Attrs = getTermAttributes();
    }
    menu_start = sh_Attrs->numrows;

    dumpRows(menu, 0, sh_Attrs->numrows);
    setAppMessage(">>>>>>>>>>>>>>> SELECT CUSTOM TEST <<<<<<<<<<<<<<<");

    int menu_end = sh_Attrs->numrows;
    uint8_t still_browsing = 0;
    DIR *d;
    struct dirent *dir;
    char *baseWorkingDir = getcwd(NULL, 0);
    d = opendir(testDir);

    chdir(testDir);

    do {
        uint32_t fileCount = 0;
        /* Zero initialize files array. */
        char *files[1000] = {};
        struct stat statbuf = {0};

        if (d)
        {
            while ((dir = readdir(d)) != NULL)
            {
                if (filterFiles(dir->d_name) == PASS)
                {
                    char type = 'f';

                    stat(dir->d_name, &statbuf);

                    if (S_ISDIR(statbuf.st_mode))
                        type = 'd';

                    asprintf(&message, "%d -> %s [%c]\n", fileCount, dir->d_name, type);
                    dumpRows(message, 0, sh_Attrs->numrows);
                    free(message);

                    /* Register the file name. */
                    files[fileCount++] = dir->d_name;
                }
            }
        }

        /* Check the choice. */
        {
            uint32_t chosenByCursor = 0;
            uint8_t digits = 0;
            uint8_t cnt = 0;
            uint32_t temp = fileCount;
            uint32_t current = menu_end;

            rowAppendString(&sh_Attrs->row[current], "\xe2\x86\x90", 3);

            dumpRows("Type the number of the file/dir you want to select.", 0, sh_Attrs->numrows);

            /* Count how many digits fileCount has. */
            while (temp)
            {
                temp /= 10;
                digits++;
            }

            char *response = (char *)calloc(digits ,1);
            int c = 0;
            uint32_t convertToInt = 0;

            while (cnt < digits && c != '\r')
            {
                while (((c = getKey()) < 48 || c > 58)
                       && c != '\r'
                       && c != ARROW_DOWN
                       && c != 'j'
                       && c != ARROW_UP
                       && c != 'k'
                      );

                if( c >= 48 && c < 58 )
                {
                    response[cnt++] = c - 48;
                    insertChar(c);
                }
                else if (c == ARROW_DOWN
                        || c == 'j'
                        || c == ARROW_UP
                        || c == 'k'
                        )
                {
                    moveBrowseCursor(menu_end, menu_end + fileCount - 1, &current, c, sh_Attrs);
                }
                else
                {
                    chosenByCursor = 1;
                }
            }

            if ( !chosenByCursor )
                for (cnt = 0; cnt < digits; cnt++)
                    convertToInt = 10 * convertToInt + response[cnt];
            else
                convertToInt = current - menu_end;

            free(response);

            if (convertToInt < fileCount)
            {
                stat(files[convertToInt], &statbuf);

                if (S_ISDIR(statbuf.st_mode))
                {
                    closedir(d);
                    d = opendir(files[convertToInt]);
                    chdir(files[convertToInt]);
                    still_browsing = 1;
                    delRows(menu_end);
                }
                else
                {
                    cTest->text = fileToBuffer(files[convertToInt]);
                    forCleanup(cTest->text);

                    /* Make the test name to be the hash of its file name. */
                    cTest->testName = calloc(SHA_DIGEST_LENGTH + 1, 1);
                    /* Need to add a way to retrieve test results from hashes (add sql queries in speed_test_sqlite.c)*/
                    SHA1((unsigned char *)cTest->testName, SHA_DIGEST_LENGTH, (unsigned char *)files[convertToInt]);
                    forCleanup(cTest->testName);

                    /* End the loop. */
                    still_browsing = 0;

                    /* Return to the original directory. */
                    chdir(baseWorkingDir);

                    /* This pointer needs to be freed by us. */
                    free(baseWorkingDir);

                    /* Close the directory so it can be reopened in succesive calls(if needed). */
                    closedir(d);

                    /* Delete all the outpute associated with this function . */
                    delRows(menu_start);
                }
            }
        }
    } while ( still_browsing );
}
