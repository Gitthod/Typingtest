#ifndef _MYSTRINGS_H_123
#define _MYSTRINGS_H_123

#include <stdint.h>

/* This function behaves exactly like strcmp with the exception that it can be given an extra argument to restrict
 * the comparison inside that length.
 */
int mStrCmp(const char* str1, const char* str2, uint32_t len);

/* replace inside the string all the occurrences of pattern with replacement. */
/* The result needs to be freed. */
char * substitutePattern(const char *source, const char *pattern, const char *replacement);

#endif
