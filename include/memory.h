#ifndef MEMORY_H_123
#define MEMORY_H_123
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/*
 * Frees all the pointers registered with forCleanup function.
 */
void freeAll(void);

/*
 * Registers the pointer in memory so it can be freed later with freeAll.
 */
void forCleanup(void *ptr);
#endif
