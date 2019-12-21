#include "argParse.h"
#include <string.h>
#include "stdlib.h"
#include <stdio.h>


/* ------------------------------------------------------------------------------------------------------------------ */
/* --------------------------------------------- Static Function Prototypes ----------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

/* This function will print the way the program can be invoked based on the registered arguments */
static char * helpMessage(void);

/* Check that the registered arguments are valid */
/* The function returns a pointer to an error message if 2 arguments have the same names */
/* In other case it returns a null pointer. */
static char * validateArguments(void);

/* The function returns the address of the element in registeredArguments array if it exists. */
/* If it doesn't exist it returns a null pointer. */
static argument * isArgumentValid(char *argName);

/* ------------------------------------------------------------------------------------------------------------------ */
/* ------------------------------------------------ Static Variables ------------------------------------------------ */
/* ------------------------------------------------------------------------------------------------------------------ */

/************** POSITIONAL ARGUMENTS **************/
/* A pointer to the positional arguments corresponding to their position of invokation */
static positionalArgument *positionalArguments = 0;

/* The number of the positional arguments */
static uint32_t numberOfPosArgs = 0;

/************** OPTIONAL ARGUMENTS ****************/
/* The following array contains the information for every user registered argument. */
static argument *registeredArguments = 0;

/* The following array is correspondent to the previous array but it contains the values of each argument. */
/* If an argument is in position i in the previous array then its value is in position i in the next array. */
static char **registeredValues = 0;

/* The length of the previous arrays. */
static uint32_t argumentsLength = 0;

/* Reserved address to denote an enabled switch. */
static char c[1];

/* ------------------------------------------------------------------------------------------------------------------ */
/* --------------------------------------------- Global Function Definition ----------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

char * registerArguments(argument *arguments, uint32_t lengthOfArray)
{
    /* Initialize the number of arguments. */
    argumentsLength = lengthOfArray;

    /* Allocate memory for the argument structs. */
    registeredArguments = malloc(lengthOfArray * sizeof(argument));

    /* Allocate memory for the values of each argument. */
    registeredValues = calloc(lengthOfArray , sizeof(char*));

    /* Populate the array. */
    for (int i = 0; i < lengthOfArray; i++)
        memcpy(&registeredArguments[i], &arguments[i], sizeof(argument));

    return validateArguments();
}

char* registerPositionalArguments(positionalArgument *arguments, uint32_t lengthOfArray)
{
    for (int i = 0; i < lengthOfArray; i++)
    {
        for (int j = i + 1; j < lengthOfArray; j++)
        {
            if (strcmp(arguments[i].name, arguments[j].name) == 0)
            {
                char *message;
                asprintf(&message, "Error! The following arguments have identical names:"
                                   "arg%d : %s\n"
                                   "arg%d : %s\n", i, arguments[i].name, j, arguments[j].name);
                return message;
            }
        }
    }

    /* Initialize the number of arguments. */
    numberOfPosArgs = lengthOfArray;

    /* Allocate memory for the argument structs. */
    positionalArguments = malloc(lengthOfArray * sizeof(positionalArgument));

    /* Populate the array. */
    for (int i = 0; i < lengthOfArray; i++)
        memcpy(&positionalArguments[i], &arguments[i], sizeof(positionalArgument));

    return NO_ERROR;
}

char * parserUserInput(char **argv, int argc)
{
    /* Index of the positional argument. */
    uint32_t posIdx = 0;
    for (int i = 1; i < argc; i ++)
    {

        /* Every user typed argument that doesn't begin with - is considered a positional argument. */
        if (argv[i][0] != '-')
        {
            if (numberOfPosArgs > posIdx)
            {
                positionalArguments[posIdx++].value = argv[i];
            }
            else
            {
                char *message;
                asprintf(&message, "The number of positional arguments is %u.\n"
                                   "This is the extra argument %s", numberOfPosArgs, argv[i]);
                return message;
            }
        }

        char *pos = 0;
        /* Check if the argument is a switch. */
        if ((pos = strchr(argv[i], '=')) ==  NULL)
        {
            argument *idx = isArgumentValid(argv[i]);

            if (strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0)
                return helpMessage();

            if (idx != 0)
            {
                if (idx->type == argBinary)
                {
                    uint32_t valueIdx = 0;
                    valueIdx = (idx - registeredArguments) / sizeof(argument);
                    registeredValues[valueIdx] = c;
                }
                else
                {
                    char *message;
                    asprintf(&message, "Error Parsing %s!\n"
                                       "This argument needs a value.\n",argv[i]);
                    return message;
                }
            }
            else
            {
                char *message;
                asprintf(&message, "Error Parsing %s!\n"
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
                    registeredValues[valueIdx] = pos + 1;
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

argValue getArgValue(char *name)
{
    argValue res = {ARG_NOT_OK, NULL};
    for (int i = 0; i < numberOfPosArgs; i++)
        if (strcmp(positionalArguments[i].name, name) == 0)
        {
            res.pointerValid = ARG_OK;
            res.value = positionalArguments[i].name;
            return res;
        }

    for (int i = 0; i < argumentsLength; i++)
        if (strcmp(registeredArguments[i].fullName, name) == 0 ||
            strcmp(registeredArguments[i].shortName, name) == 0)
        {
            res.pointerValid = ARG_OK;
            res.value = registeredValues[i];
            return res;
        }

    return res;
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* --------------------------------------------- Static Function Definition ----------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

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

static char * validateArguments(void)
{
    for (int i = 0; i < argumentsLength - 1; i++)
    {
        for (int j = i + 1; j < argumentsLength; j++)
        {
            if (0 == strcmp(registeredArguments[j].shortName, registeredArguments[i].shortName))
            {
                char *message;
                asprintf(&message, "Error! The following arguments have identical short names:\n"
                                   "arg%d : %s\n"
                                   "arg%d : %s\n", i, registeredArguments[i].shortName
                                                 , j, registeredArguments[j].shortName);
                return message;
            }

            if (0 == strcmp(registeredArguments[j].fullName, registeredArguments[i].fullName))
            {
                char *message;
                asprintf(&message, "Error! The following arguments have identical full names:\n"
                                   "arg%d : %s\n"
                                   "arg%d : %s\n", i, registeredArguments[i].fullName
                                                 , j, registeredArguments[j].fullName);

                return message;
            }
        }
    }

    return NO_ERROR;
}

static char * helpMessage(void)
{
    char     *message             = 0;
    char     *flags               = calloc(sizeof(char), 1);
    char     *description         = calloc(sizeof(char), 1);
    char     *posDescription      = calloc(sizeof(char), 1);
    char     *positionalArgs      = calloc(sizeof(char), 1);

    /* Contains the length of the flag names in short form. */
    uint32_t flagLength           = 0;
    /* Contains the length of the positional argument names. */
    uint32_t posLength            = 0;
    /* Contains the help message for all the optional arguments. */
    uint32_t descriptionLength    = 0;
    /* Contains the help message for all the positional arguments. */
    uint32_t posDescriptionLength = 0;

    for (int i = 0; i < numberOfPosArgs; i++)
    {
        /* Holds the explanation of the current positional argument. */
        char *descriptionPart = 0;
        uint32_t nameLength = strlen(positionalArguments[i].name);
        uint32_t descriptionPartLength = 0;

        asprintf(&descriptionPart, "%s\n\t%s\n\n", positionalArguments[i].name, positionalArguments[i].explanation);
        descriptionPartLength = strlen(descriptionPart);

        posDescriptionLength += descriptionPartLength;
        posLength += nameLength + 1;
        posDescription = realloc(posDescription, posDescriptionLength);
        positionalArgs = realloc(positionalArgs, posLength);

        memcpy(positionalArgs + posLength - (nameLength + 1), positionalArguments[i].name, nameLength);
        memcpy(posDescription + posDescriptionLength - descriptionPartLength, descriptionPart, descriptionPartLength);
        free(descriptionPart);

        /* Check whether to terminate or insert a space. */
        if ( i == argumentsLength - 1)
        {
            positionalArgs[posLength - 1] = 0;
        }
        else
            positionalArgs[posLength - 1] = ' ';
    }

    for (int i = 0; i < argumentsLength; i++)
    {
        char *descriptionPart = 0;
        uint32_t shortNameLength = strlen(registeredArguments[i].shortName);
        uint32_t descriptionPartLength = 0;


        asprintf(&descriptionPart, "%s, %s\n\t%s\n\n", registeredArguments[i].shortName, registeredArguments[i].fullName,
                registeredArguments[i].explanation);
        descriptionPartLength = strlen(descriptionPart);

        /* The plus one is needed either to insert space between the arguments or null terminate. */
        flagLength += shortNameLength + 1;
        descriptionLength += descriptionPartLength;

        flags = realloc(flags, flagLength);
        description = realloc(description, descriptionLength);

        memcpy(flags + flagLength - (shortNameLength + 1), registeredArguments[i].shortName, shortNameLength);
        memcpy(description + descriptionLength - descriptionPartLength, descriptionPart, descriptionPartLength);

        /* Free the partial description. */
        free(descriptionPart);

        /* Check whether to terminate or insert a space. */
        if ( i == argumentsLength - 1)
        {
            flags[flagLength - 1] = 0;
            /* Remove the last newline. */
            description[descriptionLength - 1] = 0;
        }
        else
            flags[flagLength - 1] = ' ';
    }

    asprintf(&message, "Usage example:\n"
                       "<prog_name> [%s] %s\n\n%s%s", flags, positionalArgs, posDescription, description);

    free(flags);
    free(description);
    free(posDescription);
    free(positionalArgs);

    return message;
}
