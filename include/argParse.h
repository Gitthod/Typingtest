#ifndef _ARGPARSE_H_123
#define _ARGPARSE_H_123
#include <stdint.h>

#define UNITIALIZED   0U
#define ARG_OK        0U
#define ARG_NOT_OK    0U

typedef union argValue {
    uint32_t switchStatus;
    char *value;
} argValue;

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
} argument;

typedef struct positionalArgument {
    char *name;
    char *explanation;
    char *value;
} positionalArgument;

/* This function registers the optional arguments which can be flags or optional named arguments. */
char * registerArguments(argument *arguments, uint32_t lengthOfArray);

/* This funtion registers the positional and obligatory arguments of the function. */
void registerPositionalArguments(positionalArgument *arguments, uint32_t lengthOfArray);

char * parserUserInput(char **argv, int argc);

argValue getArgValue(char *name);
#endif
