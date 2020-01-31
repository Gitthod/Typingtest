#ifndef _SPEED_BROWSER_H_123
#define _SPEED_BROWSER_H_123

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Global Function Declarations ---------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

/*
 * Browse through the cli given directory to select a test(file).
 */
void selectTest(void);

/* Set the directory to open when browsing. */
void changeTestDir(char *dirName);
#endif