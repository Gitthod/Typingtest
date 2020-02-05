#include "speed_browser.h"
#include "speed_test.h"
#include "unistd.h"
#include <dirent.h>
#include <memory.h>
#include <openssl/sha.h>
#include <raw_term.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------------------------------------------------------ */
/* ---------------------------------------------------- Defines ----------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

#define FILTERED_OUT            1
#define PASS                    0
#define BROWSE_MARKER           " \x1b[36m<==\x1b[0m"
#define BROWSE_MARKER_SIZE      13
#define MAXIMUM_FILENAME_LENGTH 255

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

/* The default directory to be opened containing typing tests. */
static char *testDir = "tests";

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Static Function Declarations ---------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

/* Moves the cursor when selecting files. */
static cursorStatus moveBrowseCursor(uint32_t min, uint32_t max, int *current, int movement, termAttributes *E);

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

static cursorStatus moveBrowseCursor(uint32_t min, uint32_t max, int *current, int movement, termAttributes *E)
{
    char *cursor = BROWSE_MARKER; /* Leftward arrow. */
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
        rowTruncateString(&E->row[*current], BROWSE_MARKER_SIZE);

        if (movement == ARROW_DOWN || movement == 'j')
        {
            rowAppendString(&E->row[++(*current)], cursor, BROWSE_MARKER_SIZE);
        }
        else
        {
            rowAppendString(&E->row[--(*current)], cursor, BROWSE_MARKER_SIZE);
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
    char *message;
    int menu_end;

    char *menu = "Select the file containing the text you want to be tested upon\n"
                 "##############################################################\n";

    if (!sh_Attrs)
    {
        sh_Attrs = getTermAttributes();
    }
    menu_start = sh_Attrs->numrows;

    dumpRows(menu, 0, menu_start);
    menu_end = sh_Attrs->numrows;

    uint8_t still_browsing = 0;
    DIR *d;
    struct dirent *dir;
    char *baseWorkingDir = getcwd(NULL, 0);
    char *mostRecentLongestPath = 0;

    if ( (d = opendir(testDir)) )
        chdir(testDir);
    else
        d = opendir("./");

    mostRecentLongestPath = getcwd(NULL, 0);

LOOP_START:
    do
    {
        uint32_t fileCount = 0;
        /* Zero initialize files array. */
        /* These pointers don't need to be freed. */
        char *files[1000] = {};
        struct stat statbuf = {};
        still_browsing = 0;

        delRows(menu_end);
        if (d)
        {
            char *test_message;
            asprintf(&test_message, "The current dir is\x1b[36m\t\t%s\x1b[0m", getcwd(NULL, 0));
            dumpRows(test_message, 1, sh_Attrs->numrows);
            free(test_message);

            /* This is required in the cased that d wasn't updated and the process is repeated. */
            rewinddir(d);

            while ((dir = readdir(d)) != NULL)
            {
                if (filterFiles(dir->d_name) == PASS)
                {
                    char *type = "\x1b[32mf\x1b[0m";

                    stat(dir->d_name, &statbuf);

                    if (S_ISDIR(statbuf.st_mode))
                        type =  "\x1b[36md\x1b[0m";

                    asprintf(&message, "%d -> %s [%s]\n", fileCount, dir->d_name, type);
                    dumpRows(message, 0, sh_Attrs->numrows);
                    free(message);

                    /* Register the file name. */
                    files[fileCount++] = dir->d_name;
                }
            }
        }
        else
        {
            pexit("Directory doesn't exist\n");
        }

        /* Check the choice. */
        {
            uint32_t chosenByCursor = 0;
            uint8_t digits = 0;
            uint8_t cnt = 0;
            uint32_t temp = fileCount;

            rowAppendString(&sh_Attrs->row[sh_Attrs->cy - 1], BROWSE_MARKER, BROWSE_MARKER_SIZE);
            sh_Attrs->cy -= 1;
            setAppMessage("Type a number between 0 - %d, or press Enter to select the test under the cursor.\n"
                          "Press h to go to the parent dir (../) or l to go to the previous one.\n",
                          fileCount);
            appendAppMessage("You typed:");

            /* Count how many digits fileCount has. */
            while (temp)
            {
                temp /= 10;
                digits++;
            }

            char *response = (char *)calloc(digits ,1);
            int c = 0;
            uint32_t convertToInt = 0;

            while (cnt < digits && c != '\r' && c != 'l' && c != 'h')
            {
                while (((c = getKey()) < 48 || c > 58)
                        && c != '\r'
                        && c != ARROW_DOWN
                        && c != 'j'
                        && c != ARROW_UP
                        && c != 'k'
                        && c != 'l' /* Go to the previous directory. */
                        && c != 'h' /* Go to the parent directory. */
                      );

                if( c >= 48 && c < 58 )
                {
                    response[cnt++] = c;
                    /* c has a size of int due to this app's limitations it's guaranteed that it will be null terminated.
                     */
                    appendAppMessage((char *)&c);
                }
                else if (c == ARROW_DOWN
                        || c == 'j'
                        || c == ARROW_UP
                        || c == 'k'
                        )
                {
                    moveBrowseCursor(menu_end + 1, menu_end + fileCount, &sh_Attrs->cy, c, sh_Attrs);
                }
                /* Enter  or l was pressed. */
                else if ( c == '\r' )
                {
                    /* If no number was pressed choose the file under the cursor. */
                    if ( cnt == 0 )
                        chosenByCursor = 1;
                }
                else if ( c == 'h' )
                {
                    closedir(d);
                    d = opendir("..");
                    chdir("..");
                    goto LOOP_START;
                }
                else if ( c == 'l' )
                {
                    char *curDir = getcwd(NULL, 0);
                    char *idxC = curDir;
                    char *idxLP = mostRecentLongestPath;
                    char *nextDir = 0;

                    if (mostRecentLongestPath != 0 &&
                        strcmp(curDir, mostRecentLongestPath) != 0)
                    {
                        while (*idxC++ == *idxLP++);

                        if ( *(idxLP - 1) != 0)
                        {
                            /* This covers the edge case where the curDir ends with / */
                            /* This is only possible when the current dir is the root dir */
                            if (*(idxLP - 2) == '/')
                                idxLP--;
                            nextDir = calloc(MAXIMUM_FILENAME_LENGTH + 1, 1);
                            char *tmpIdx = idxLP;
                            while (*tmpIdx != '/' && *tmpIdx != 0)
                                tmpIdx++;

                            memcpy(nextDir, idxLP, tmpIdx - idxLP);
                            free(curDir);
                        }
                        else
                        {
                            nextDir = curDir;
                        }
                    }
                    else
                        free(curDir);

                    if (nextDir && *nextDir)
                    {
                        closedir(d);
                        d = opendir(nextDir);
                        if (d == 0)
                        {
                            pexit("Couldn't open %s directory\n", nextDir);
                        }
                        chdir(nextDir);
                        free(nextDir);
                    }

                    goto LOOP_START;
                }
                else
                {
                    /* Shouldn't be able to go there. */
                    pexit("selectTest");
                }
            }

            if ( !chosenByCursor )
                for (int i = 0; i < cnt; i++)
                    convertToInt = 10 * convertToInt + response[i] - 48;
            else
                convertToInt = sh_Attrs->cy - (menu_end + 1);

            free(response);

            if (convertToInt < fileCount)
            {
                stat(files[convertToInt], &statbuf);

                if (S_ISDIR(statbuf.st_mode))
                {
                    closedir(d);
                    d = opendir(files[convertToInt]);
                    chdir(files[convertToInt]);

                    char *curDir = getcwd(NULL, 0);
                    /* Check if the current directory is a subdirectory of the Longest Path. */
                    if ( mostRecentLongestPath == 0 || strncmp(curDir, mostRecentLongestPath, strlen(curDir)))
                    {
                        free(mostRecentLongestPath);
                        mostRecentLongestPath = curDir;
                    }
                    else
                        free(curDir);

                    still_browsing = 1;
                }
                else
                {
                    cTest->text = fileToBuffer(files[convertToInt]);
                    forCleanup(cTest->text);

                    /* Make the test name to be the hash of its file name. */
                    cTest->testName = calloc(SHA_DIGEST_LENGTH + 1, 1);
                    /* Need to add a way to retrieve test results from hashes (add sql queries in speed_test_sqlite.c)*/
                    SHA1((unsigned char *)files[convertToInt], SHA_DIGEST_LENGTH, (unsigned char *)cTest->testName);

                    forCleanup(cTest->testName);

                    /* Copy the file name. */
                    {
                        uint16_t fileNameLength = 0;
                        char *tmp = files[convertToInt];

                        while (*tmp++)
                            fileNameLength++;

                        cTest->fileName = calloc(fileNameLength + 1, 1);

                        for (int i = 0; i < fileNameLength; i++)
                            cTest->fileName[i] = files[convertToInt][i];

                        forCleanup(cTest->fileName);
                    }

                    /* End the loop. */
                    still_browsing = 0;

                    /* Close the directory so it can be reopened in succesive calls(if needed). */
                    closedir(d);

                    /* Return to the original directory. */
                    chdir(baseWorkingDir);

                    /* This pointer needs to be freed by us. */
                    free(baseWorkingDir);

                    free(mostRecentLongestPath);

                    /* Delete all the outpute associated with this function . */
                    delRows(menu_start);
                }
            }
            else
            {
                still_browsing = 1;
            }
        }
    } while ( still_browsing );
}

void changeTestDir(char *dirName)
{
    testDir = dirName;
}
