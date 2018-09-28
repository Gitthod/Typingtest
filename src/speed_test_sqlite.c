#include <speed_test_sqlite.h>

/* Static functions. */
static void deinitSQLite(void);

/* Static variables. */
static termAttributes *sh_Attrs;
static sqlite3 *db;

/*
 * This funtion needs to be called before sqlite operations.
 * It initializes the db and sh_Attrs pointers.
 */
int init_sqlite_db(void)
{
    char *err_msg = 0;
    sqlite3_stmt *res;
    sh_Attrs = getTermAttributes();

    int rc = sqlite3_open("test.db", &db);

    if (rc != SQLITE_OK) {

        asprintf(&err_msg, "Cannot open database: %s\n",
                sqlite3_errmsg(db));
        dumpRows(err_msg, 0, sh_Attrs->numrows);
        free(err_msg);
        sqlite3_close(db);

        return 1;
    }

    char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='Records';";
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc != SQLITE_OK)
    {
        asprintf(&err_msg, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        dumpRows(err_msg, 0, sh_Attrs->numrows);
        free(err_msg);
        sqlite3_close(db);

        return 1;
    }

    int step = sqlite3_step(res);
    sqlite3_finalize(res);
    if (step != SQLITE_ROW)
    {
        sql =  "CREATE TABLE Records(Id INTEGER PRIMARY KEY, Fingers TEXT,\
            Length INT, Mistakes INT, Time REAL);";
        rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

        if (rc != SQLITE_OK )
        {

            char *message = 0;
            asprintf(&message, "SQL error: %s\n", err_msg);
            dumpRows(message, 0, sh_Attrs->numrows);
            free(message);

            sqlite3_free(err_msg);
            sqlite3_close(db);

            return 1;
        }
            sqlite3_free(err_msg);
    }
    atexit(deinitSQLite);
    return 0;
}

static void deinitSQLite(void)
{
    sqlite3_close(db);
}
int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    NotUsed = 0;
    for (int i = 0; i < argc; i++)
    {
        char *message = 0;
        asprintf(&message, "%s = %s\n", azColName[i], argv[i] ?  argv[i] : "NULL");
        dumpRows(message, 0, sh_Attrs->numrows);
        free(message);
    }

    dumpRows("\n", 0, sh_Attrs->numrows);

    return 0;
}

int insert(char* Fingers, int Length, int Mistakes, float Time)
{
    char *sql;
    int rc;
    char *err_msg = 0;

    asprintf(&sql, "INSERT INTO Records(Fingers, Length, Mistakes, Time) \
            VALUES('%s', '%d', '%d', '%f');", Fingers, Length, Mistakes, Time);
    rc = sqlite3_exec(db, sql, callback, 0, &err_msg);
    free(sql);

    if (rc != SQLITE_OK )
    {
        char *message = 0;
        asprintf(&message, "SQL error: %s\n", err_msg);
        dumpRows(message, 0, sh_Attrs->numrows);
        free(message);

        sqlite3_free(err_msg);
        sqlite3_close(db);

        return 1;
    }

    return 0;
}

int get_nmin(char *Fingers, int Length, char no_of_results)
{
    int rc;
    char *err_msg = 0;
    char *sql;

    /*
     * Check if the test belongs to the default 2finger tests, if it doesn't diregard Length.
     */
    if (strcmp(Fingers,"op") == 0 ||
            strcmp(Fingers,"l;") == 0 ||
            strcmp(Fingers,"./") == 0 ||
            strcmp(Fingers,"qw") == 0 ||
            strcmp(Fingers,"as") == 0 ||
            strcmp(Fingers,"zx") == 0 )
    {
        asprintf(&sql, "SELECT Fingers, Length, Mistakes, ROUND(Time, 2) as Time, \
                ROUND(Length / Time * 60, 2)  as CPM FROM Records WHERE Fingers='%s' AND \
                Length ='%d' order by Time asc limit %c;", Fingers, Length, no_of_results);
    }
    else
    {
        asprintf(&sql, "SELECT Fingers, Length, Mistakes, ROUND(Time, 2) as Time, \
                ROUND(Length / Time * 60, 2)  as CPM FROM Records WHERE Fingers='%s'\
                order by Time asc limit %c;", Fingers, no_of_results);
    }

    rc = sqlite3_exec(db, sql, callback, 0, &err_msg);
    free(sql);

    if (rc != SQLITE_OK )
    {

        char *message = 0;
        asprintf(&message, "SQL error: %s\n", err_msg);
        dumpRows(message, 0, sh_Attrs->numrows);
        free(message);

        sqlite3_free(err_msg);
        sqlite3_close(db);

        return 1;
    }

    return 0;
}

int get_average(char *Fingers, int Length)
{
    int rc;
    char *err_msg = 0;
    char *sql = 0;

    /*
     * Check if the test belongs to the default 2finger tests, if it doesn't diregard Length.
     */
    if (strcmp(Fingers,"op") == 0 ||
            strcmp(Fingers,"l;") == 0 ||
            strcmp(Fingers,"./") == 0 ||
            strcmp(Fingers,"qw") == 0 ||
            strcmp(Fingers,"as") == 0 ||
            strcmp(Fingers,"zx") == 0 )
    {
        asprintf(&sql,"SELECT Fingers, Length, ROUND(AVG(Time), 2) AS [Average Time], \
                ROUND(AVG(Mistakes), 2) AS [Average mistakes per test] FROM Records WHERE FINGERS='%s' \
                AND Length='%d';", Fingers, Length);
    }
    else
    {
        asprintf(&sql,"SELECT Fingers, Length, ROUND(AVG(Time), 2) AS [Average Time], \
                ROUND(AVG(Mistakes), 2) AS [Average mistakes per test] FROM Records \
                WHERE FINGERS='%s';", Fingers);
    }

    rc = sqlite3_exec(db, sql, callback, 0, &err_msg);
    free(sql);

    if (rc != SQLITE_OK )
    {
        char *message = 0;
        asprintf(&message, "SQL error: %s\n", err_msg);
        dumpRows(message, 0, sh_Attrs->numrows);
        free(message);

        sqlite3_free(err_msg);
        sqlite3_close(db);

        return 1;
    }

    return 0;
}

int get_all_averages(char *Fingers)
{
    int rc;
    char *err_msg = 0;
    char *sql = 0;
    if (Fingers)
        asprintf(&sql,"SELECT Fingers, Length, ROUND(AVG(Time), 2) AS [Average Time], ROUND(AVG(Mistakes), 2) AS\
                [Average mistakes per test],  ROUND(Length / AVG(Time) * 60, 2) as [Clicks per minute], \
                ROUND(AVG(Mistakes) / Length * 100, 2) as [Mistakes per 100 clicks] FROM Records \
                WHERE FINGERS='%s' GROUP BY Length;", Fingers);
    else
        asprintf(&sql,"SELECT COUNT(Id) as [Tests Taken], Sum(Length) as [Total characters typed], Fingers, \
                ROUND(Sum(Length) / Sum(Time) * 60, 2) AS [CPM], ROUND(cast(Sum(Mistakes) as FLOAT) /\
                Sum(Length) * 100, 2) AS [Mistakes per 100 key presses] FROM Records GROUP BY Fingers;");

    rc = sqlite3_exec(db, sql, callback, 0, &err_msg);
    free(sql);

    if (rc != SQLITE_OK )
    {
        char *message = 0;
        asprintf(&message, "SQL error: %s\n", err_msg);
        dumpRows(message, 0, sh_Attrs->numrows);
        free(message);

        sqlite3_free(err_msg);
        sqlite3_close(db);

        return 1;
    }

    return 0;
}
