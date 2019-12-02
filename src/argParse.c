#include "argParse.h"
#include <string.h>
#include "stdlib.h"
#include <stdio.h>


/* The function returns the address of the element in registeredArguments array if it exists. */
/* If it doesn't exist it returns a null pointer. */
static argument * isArgumentValid(char *argName);

/* The following array contains the information for every user registered argument. */
static argument *registeredArguments = 0;

/* The following array is correspondent to the previous array but it contains the values of each argument. */
/* If an argument is in position i in the previous array then its value is in position i in the next array. */
static argValue *registeredValues = 0;

/* The length of the previous arrays. */
static uint32_t argumentsLength = 0;

void registerArguments(argument *arguments, uint32_t sizeOfArray)
{
    /* Initialize the number of arguments. */
    argumentsLength = sizeOfArray;

    /* Allocate memory for the argument structs. */
    registeredArguments = malloc(sizeOfArray * sizeof(argument));

    /* Allocate memory for the values of each argument. */
    registeredValues = calloc(sizeOfArray , sizeof(argument));

    /* Populate the array. */
    for (int i = 0; i < sizeOfArray; i++)
        memcpy(&registeredArguments[i], &arguments[i], sizeof(argument));
}


char * parserUserInput(char **argv, int argc)
{
    for (int i = 1; i < argc; i ++)
    {
        /* Check if the argument is a switch. */
        char *pos = 0;
        if ((pos = strchr(argv[i], '=')) ==  0)
        {
            argument *idx = isArgumentValid(argv[i]);

            if (idx != 0)
            {
                if (idx->type == argBinary)
                {
                    uint32_t valueIdx = 0;
                    valueIdx = (idx - registeredArguments) / sizeof(argument);
                    registeredValues[valueIdx].switchStatus = 1;
                }
                else
                {
                    char *message;
                    asprintf(&message, "Error Prasing %s!\n"
                                       "This argument needs a value.\n",argv[i]);
                    return message;
                }
            }
            else
            {
                char *message;
                asprintf(&message, "Error Prasing %s!\n"
                                   "This argument doesn't exist!\n",argv[i]);
                return message;
            }

        }
        else
        {
            /* Allocate + 1 for null termination. */
            char *argName = calloc(pos - argv[i] + 1, sizeof(char));

            memcpy(argName, argv[i], pos - argv[i]);

            argument *idx = isArgumentValid(argName);
            free(argName);

            if (idx != 0)
            {
                if (idx->type == argVariable)
                {
                    uint32_t valueIdx = 0;
                    valueIdx = (idx - registeredArguments) / sizeof(argument);
                    registeredValues[valueIdx].value = pos + 1;
                }
                else
                {
                    char *message;
                    asprintf(&message, "Error Prasing %s!\n"
                                       "This argument is a switch!\n",argv[i]);
                    return message;
                }
            }
            else
            {
                char *message;
                asprintf(&message, "Error Prasing %s!\n"
                                   "This argument doesn't exist!\n",argv[i]);
                return message;
            }
        }
    }

    return 0;
}

static argument * isArgumentValid(char *argName)
{
    for (int i = 0; i < argumentsLength; i++)
        if (strcmp(registeredArguments[i].fullName, argName) == 0 ||
                strcmp(registeredArguments[i].shortName, argName) == 0)
        {
            return &registeredArguments[i];
        }

    return 0;
}

argValue getArgValue(char *name)
{
    argument *idx = isArgumentValid(name);
    uint32_t valueIdx = 0;

    if (idx != 0)
        valueIdx = (idx - registeredArguments) / sizeof(argument);

    return registeredValues[valueIdx];
}
