#ifndef SPEED_TEST_SQLITE_H_123
#define SPEED_TEST_SQLITE_H_123


/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Global Function Declarations ---------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

/*
 * Gets the results returned by the sqlite query and prints them.
 */
int callback(void *, int, char **, char **);

/*
 * Create or connect to the sqlite database, and handle errors related to these operations.
 */
int init_sqlite_db(void);

/*
 * Queries the database to get the tests with test name *fingers, and length Length
 * you can order up to no_of_results results.
 */
int get_nmin(char *Fingers, int Length, char no_of_results);

/*
 * Inserts a result to the sqlite database.
 */
int insert(char *name, char *test, int Length, int Mistakes, float Time, char hash[40]);

/*
 * Get the average results for a single test.
 */
int get_average(char *Fingers, int Length);

/*
 * Get the average results of all the tests stored in the database.
 */
int get_all_averages(char *Fingers);

/* If hash is null the function will query the database for filename. If there are multiple filenames it will return
 * with an error. If the filename is unique then it will create a new file if the file destFilename doesn't exist
 * otherwise it will print the error and return.
 * If the hash is passed it will be given to the database to match LIKE 'hash%' so only the first unique digits are
 * really needed. The initial process will be applied also in this situation to save the contents under destFilename.
 */
int saveContentToFile(char *filename, char *hash, char *destFilename);
#endif
