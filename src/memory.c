#include <memory.h>


/* This keeps track of pointers that need to be freed. */
static void **root = 0;

void forCleanup(void *ptr)
{

    if (NULL == root)
        root = calloc(sizeof(void *), 1);

    if (NULL == ptr)
    {
        return;
    }
    else
    {
        /* The array needs at least one NULL pointer. */
        static int size = 1;

        /* Check if the pointer is already registered. */
        for(int i = 0; i < size; i++)
            if (root[i] == ptr)
                return;

        /* Avoid warning regarding not error handling realloc with some linters. */
        void **tmp = root;
        root = realloc(root, sizeof(void*) * (size + 1));

        if (NULL == root)
        {
            free(tmp);
            perror("forCleanup");
        }

        root[size] = NULL;
        root[size++ - 1] = ptr;
    }
}

void freeAll(void)
{
    void **start = root;

    if (root)
        while(*root)
        {
            free(*root++);
        }

    free(start);
}
