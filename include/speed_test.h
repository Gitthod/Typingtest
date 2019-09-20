#ifndef SPEED_TEST_H_123
#define SPEED_TEST_H_123

/*
 * Prints the main menu message and handles the user's decisions.
 */
int goto_Menu(void);

/*
 * Sets these attrbites which are static variables in the speed_test.c file.
 */
void setAttributes(int testLength, char *testName, char *fileBuffer, char ignoreWhiteSpace);

/*
 * Converts a string to a positive int. If the input isn't a number the function returns -1.
 */
int convertInput(char* input);

/*
 * Reads filename, if the file exists the function allocates memory equal to its size and returns that pointer. If the
 * file doesn't exists the function returns 0.
 */
char *fileToBuffer(char *filename);

#endif
