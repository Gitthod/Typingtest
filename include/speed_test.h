#ifndef SPEED_TEST_H_123
#define SPEED_TEST_H_123

#define _GNU_SOURCE
#include <sys/time.h>
#include <unistd.h>
#include <speed_test_sqlite.h>
#include <raw_term.h>
#include <stdio.h>

/*
 * Prints the main menu message and handles the user's decisions.
 */
int goto_Menu(void);

/*
 * Sets these attrbites which are static variables in the speed_test.c file.
 */
void setAttributes(int testLength, char *testName, char *fileBuffer);

/*
 * Converts a string to a positive int. If the input isn't a number the function returns -1.
 */
int convertInput(char* input);

/*
 * Reads filename, if the file exists the function allocates memory equal to its size and returns that pointer. If the
 * file doesn't exists the function returns 0.
 */
char *fileToBuffer(char *filename);

/*
 * Frees all the pointers registered with forCleanup function.
 */
void freeAll(void);

/*
 * Registers the pointer in memory so it can be freed later with freeAll.
 */
void forCleanup(void *ptr);
#endif
