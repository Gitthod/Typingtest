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
int insert(char* Fingers, int Length, int Mistakes, float Time);

/*
 * Get the average results for a single test.
 */
int get_average(char *Fingers, int Length);

/*
 * Get the average results of all the tests stored in the database.
 */
int get_all_averages(char *Fingers);

#endif
