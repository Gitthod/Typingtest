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

#define FILTERED_OUT 1
#define PASS         0

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

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Global Function Implementations ------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

void selectTest(void)
{
    int menu_start;
    currentTest *cTest = getCurrentTest();
    char *testDir = "tests";

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
                char *message;
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
            uint8_t digits = 0;
            uint8_t cnt = 0;
            uint32_t temp = fileCount;

            /* Count how many digits fileCount has. */
            while (temp)
            {
                temp /= 10;
                digits++;
            }

            char *response = (char *)malloc(digits);
            char c = 0;
            uint32_t convertToInt = 0;

            while (cnt < digits && c != '\r')
            {
                /* Ignore non number characters */
                while (((c = getKey()) < 48 || c > 58) && c != '\r');
                if( c != '\r')
                    response[cnt++] = c - 48;

                /* Insert the response here to make it more interactive. */
            }

            for (cnt = 0; cnt < digits; cnt++)
                convertToInt = 10 * convertToInt + response[cnt];

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
