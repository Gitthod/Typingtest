#include "myStrings.h"
#include <string.h>
#include <stdlib.h>


char * substitutePattern(const char *source, const char *pattern, const char *replacement)
{
    uint32_t patLenght = strlen(pattern);
    uint32_t repLenght = strlen(replacement);
    uint32_t sourceLength = strlen(source);
    uint32_t patternsMatched = 0;
    char *result = 0;

    /* Sanity check. */
    if (patLenght > sourceLength)
        return 0;

    for (int i = 0; i < sourceLength - patLenght; i++)
    {
        /* Check if the string at position i has the pattern. */
        if (0 == mStrCmp(&source[i], pattern, patLenght))
        {
            patternsMatched++;
            /* Skip the matched pattern. */
            i += patLenght - 1;
        }
    }

    result = calloc(sourceLength + (repLenght - patLenght) * patternsMatched + 1, 1);
    /* Generate the output string. */
    {
        const char *sidx = source;
        char *ridx = result;

        while(*sidx)
        {
            if (0 == mStrCmp(sidx, pattern, patLenght))
            {
               for (int i = 0; i < repLenght; i++)
               {
                   *ridx++ = replacement[i];
               }

               sidx += patLenght;
            }
            else
            {
                *ridx++ = *sidx++;
            }
        }
    }

    return result;
}


int mStrCmp(const char* str1, const char* str2, uint32_t len)
{
    /* If the length is zero search for null terminators. */
    char nullTerminate = !len;

    /* The short-circuit properties of && guarantee left to right execution. */
    /* Since this is a string equality comparison we only need to check if one string is null terminated (not
     * both). */
    while(*str1 && (nullTerminate | (len-- - 1))
            && (*str1 == *str2)
         )
    {
        str1++;
        str2++;
    }

    return *str1 - *str2;
}

