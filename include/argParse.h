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

void registerArguments(argument *arguments, uint32_t sizeOfArray);

char * parserUserInput(char **argv, int argc);

argValue getArgValue(char *name);
#endif
