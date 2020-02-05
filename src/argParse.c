#include "argParse.h"
#include <string.h>
#include "stdlib.h"
#include <stdio.h>

/* ------------------------------------------------------------------------------------------------------------------ */
/* --------------------------------------------- Static Function Prototypes ----------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

/* This function will print the way the program can be invoked based on the registered arguments */
static void helpMessage(char *progName);

/* Check that the registered arguments are valid */
/* The function returns a pointer to an error message if 2 arguments have the same names */
/* In other case it returns a null pointer. */
static void validateArguments(void);

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

/* The length of the previous arrays. */
static uint32_t argumentsLength = 0;

/* Check if initialization is done. */
static uint8_t initDone;

/* Register the pointer containing the description of the program. */
static char *descriptionHelp;

/* ------------------------------------------------------------------------------------------------------------------ */
/* --------------------------------------------- Global Function Definition ----------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

void registerArguments(char *description, argument *arguments, uint32_t argSize,
                         positionalArgument *posArguments, uint32_t posArgSize)
{
    /* Initialize the number of arguments. */
    argumentsLength = argSize / sizeof(argument);

    /* Allocate memory for the argument structs. */
    registeredArguments = malloc(argumentsLength * sizeof(argument));

    if (initDone)
    {
        fprintf(stderr, "Arguments are already initialized...\n");
        exit(EXIT_FAILURE);
    }
    else
        initDone = 1;

    descriptionHelp = description;

    /* Populate the array. */
    for (int i = 0; i < argumentsLength; i++)
        memcpy(&registeredArguments[i], &arguments[i], sizeof(argument));

    /* Initialize the number of arguments. */
    numberOfPosArgs = posArgSize / sizeof(positionalArgument);

    /* Allocate memory for the argument structs. */
    positionalArguments = malloc(numberOfPosArgs * sizeof(positionalArgument));

    /* Populate the array. */
    for (int i = 0; i < numberOfPosArgs; i++)
        memcpy(&positionalArguments[i], &arguments[i], sizeof(positionalArgument));

    validateArguments();
}

void parserUserInput(char **argv, int argc)
{
    if (initDone == 0)
    {
        fprintf(stderr, "registerArguments function has be invoked before the arguments can be parsed!\n");
        exit(EXIT_FAILURE);
    }

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
                fprintf(stderr, "The number of positional arguments is %u.\n"
                                   "This is the extra argument %s\n", numberOfPosArgs, argv[i]);
                exit(EXIT_FAILURE);
            }
        }

        char *pos = 0;
        /* Check if the argument is a switch. */
        if ((pos = strchr(argv[i], '=')) ==  NULL)
        {
            argument *idx = isArgumentValid(argv[i]);

            if (strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0)
            {
                /* Prints the help message and exits. */
                helpMessage(argv[0]);
            }

            if (idx != 0)
            {
                if (idx->type == argBinary)
                {
                    idx->value = NOT_NULL_ARG;
                }
                else
                {
                    fprintf(stderr, "Error Parsing %s!\n"
                                       "This argument needs a value.\n",argv[i]);
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                fprintf(stderr, "Error Parsing %s!\n"
                                   "This argument doesn't exist!\n",argv[i]);
                exit(EXIT_FAILURE);
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
                    idx->value = pos + 1;
                }
                else
                {
                    fprintf(stderr, "Error Prasing %s!\n"
                                       "This argument is a switch!\n",argv[i]);
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                fprintf(stderr, "Error Prasing %s!\n"
                                   "This argument doesn't exist!\n",argv[i]);
                exit(EXIT_FAILURE);
            }
        }
    }
}

char *getArgValue(char *name)
{
    for (int i = 0; i < numberOfPosArgs; i++)
        if (strcmp(positionalArguments[i].name, name) == 0)
            return positionalArguments[i].value;

    for (int i = 0; i < argumentsLength; i++)
        if (strcmp(registeredArguments[i].fullName, name) == 0 ||
            strcmp(registeredArguments[i].shortName, name) == 0)
            return  registeredArguments[i].value;

    fprintf(stderr, "No argument with name %s is registered\n", name);
    exit(EXIT_FAILURE);
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

static void validateArguments(void)
{
    for (int i = 0; i < argumentsLength; i++)
    {
        if (registeredArguments[i].shortName[0] != '-')
        {
            fprintf(stderr, "Optional Arguments must start with -\n"
                    "arg%d : %s is invalid!\n", i, registeredArguments[i].shortName);
            exit(EXIT_FAILURE);
        }

        if (strncmp(registeredArguments[i].fullName, "--", 2) != 0)
        {
            fprintf(stderr, "Optional Arguments full names must start with --\n"
                    "arg%d : %s is invalid!\n", i, registeredArguments[i].fullName);
            exit(EXIT_FAILURE);
        }

        for (int j = i + 1; j < argumentsLength; j++)
        {
            if (0 == strcmp(registeredArguments[j].shortName, registeredArguments[i].shortName))
            {
                fprintf(stderr, "Error! The following arguments have identical short names:\n"
                                   "arg%d : %s\n"
                                   "arg%d : %s\n", i, registeredArguments[i].shortName
                                                 , j, registeredArguments[j].shortName);
                exit(EXIT_FAILURE);
            }

            if (0 == strcmp(registeredArguments[j].fullName, registeredArguments[i].fullName))
            {
                fprintf(stderr, "Error! The following arguments have identical full names:\n"
                                   "arg%d : %s\n"
                                   "arg%d : %s\n", i, registeredArguments[i].fullName
                                                 , j, registeredArguments[j].fullName);

                exit(EXIT_FAILURE);
            }
        }
    }

    for (int i = 0; i < numberOfPosArgs; i++)
    {
        if (positionalArguments[i].name[0] == '-')
        {
            fprintf(stderr, "Positional Arguments can't start with -\n"
                    "arg%d : %s is invalid!\n", i, positionalArguments[i].name);
            exit(EXIT_FAILURE);
        }

        for (int j = i + 1; j < numberOfPosArgs; j++)
        {
            if (strcmp(positionalArguments[i].name, positionalArguments[j].name) == 0)
            {
                fprintf(stderr, "Error! The following positional arguments have identical names:"
                                   "arg%d : %s\n"
                                   "arg%d : %s\n", i, positionalArguments[i].name, j, positionalArguments[j].name);
                exit(EXIT_FAILURE);
            }
        }
    }
}

static void helpMessage(char *progName)
{
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

    fprintf(stdout, "Description:\n\t%s\n\n"
                       "Usage example:\n\t"
                       "%s [%s] %s\n\n%s%s",
                       descriptionHelp, progName, flags, positionalArgs, posDescription, description);

    free(flags);
    free(description);
    free(posDescription);
    free(positionalArgs);
    exit(EXIT_SUCCESS);
}

void freeArgs(void)
{
    free(positionalArguments);
    free(registeredArguments);
}
