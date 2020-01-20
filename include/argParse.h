#ifndef _ARGPARSE_H_123
#define _ARGPARSE_H_123
#include <stdint.h>

/* ------------------------------------------------------------------------------------------------------------------ */
/* ---------------------------------------------------- Defines ----------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

#define UNITIALIZED   0U
#define ARG_OK        1U
#define ARG_NOT_OK    0U
#define NO_ERROR      0U
/* The following address will never be accessed. The macro is created to avoid warnings. */
#define NOT_NULL_ARG (char *)1

/* ------------------------------------------------------------------------------------------------------------------ */
/* ------------------------------------------------ Type Definitions ------------------------------------------------ */
/* ------------------------------------------------------------------------------------------------------------------ */

typedef enum argType {
    argBinary,
    argVariable
} argType;

typedef struct argument {
    char *fullName;
    char *shortName;
    char *explanation;
    /* The of the argument.  */
    argType type;
    char *value;
} argument;

typedef struct positionalArgument {
    char *name;
    char *explanation;
    char *value;
} positionalArgument;

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Global Function Declarations ---------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

/* This function registers all the arguments your program supports alongside a description of what */
/* your program does. */
void registerArguments(char *description, argument *arguments, uint32_t argSize,
                         positionalArgument *posArguments, uint32_t posArgSize);

/* This function will parse the user typed arguments and extract the argument values to the memory structures. */
/* All arguments default to 0 values if they are not defined by the user. */
void parserUserInput(char **argv, int argc);

/* This function returns the value of the argument char *name. If argument name isn't valid the program will be 
   terminated. */
char *getArgValue(char *name);
#endif
