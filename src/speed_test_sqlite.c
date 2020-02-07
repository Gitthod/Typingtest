#include <raw_term.h>
#include <speed_test_sqlite.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Static Function Declarations ---------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
static void deinitSQLite(void);
static char* convertRawToText(char *base, uint32_t size);

/* ------------------------------------------------------------------------------------------------------------------ */
/* ------------------------------------------------ Static Variables ------------------------------------------------ */
/* ------------------------------------------------------------------------------------------------------------------ */
static termAttributes *sh_Attrs;
static sqlite3 *db;

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Static Function Implementations ------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

/* Free sqlite resources. */
static void deinitSQLite(void)
{
    sqlite3_close(db);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Global Function Implementations ------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

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

    /* Check whether the table Test_Records exists. */
    char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='Test_Records';";
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
        sql =  "CREATE TABLE Test_Records(filename TEXT, hash TEXT, Length INT, Mistakes INT, Time REAL,\
                PRIMARY KEY(filename, hash));";

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

    /* Check whether the table Tests exists. */
    sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='Tests';";
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    step = sqlite3_step(res);
    sqlite3_finalize(res);
    if (step != SQLITE_ROW)
    {
        sql =  "CREATE TABLE Tests(filename TEXT, contents TEXT, hash BLOB, PRIMARY KEY(filename, hash));";
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

int insert(char *name, char *test, int Length, int Mistakes, float Time, char hash[40])
{
    char *sql;
    int rc;
    char *err_msg = 0;
    sqlite3_stmt *res;

    hash = convertRawToText(hash, 40);
    asprintf(&sql,"SELECT * FROM Tests WHERE filename='%s' AND hash='%s'", name, hash);
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
    /* If the test isn't registered, register it. */
    if (step != SQLITE_ROW)
    {
        asprintf(&sql,"INSERT INTO Tests(filename, contents, hash)\
                VALUES('%s', '%s', '%s');", name, test, hash);
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

    free(sql);
    asprintf(&sql, "INSERT INTO Test_Records(filename, Length, Mistakes, Time) \
            VALUES('%s', '%d', '%d', '%f');", name, Length, Mistakes, Time);
    rc = sqlite3_exec(db, sql, callback, 0, &err_msg);

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

    free(sql);
    free(hash);
    return 0;
}

int get_nmin(char *filename, int Length, char no_of_results)
{
    int rc;
    char *err_msg = 0;
    char *sql;

    /*
     * Check if the test belongs to the default 2finger tests, if it doesn't diregard Length.
     */
    if (strcmp(filename,"op") == 0 ||
            strcmp(filename,"l;") == 0 ||
            strcmp(filename,"./") == 0 ||
            strcmp(filename,"qw") == 0 ||
            strcmp(filename,"as") == 0 ||
            strcmp(filename,"zx") == 0 )
    {
        asprintf(&sql, "SELECT filename, Length, Mistakes, ROUND(Time, 2) as Time, \
                ROUND(Length / Time * 60, 2)  as CPM FROM Test_Records WHERE filename='%s' AND \
                Length ='%d' order by Time asc limit %c;", filename, Length, no_of_results);
    }
    else
    {
        asprintf(&sql, "SELECT filename, Length, Mistakes, ROUND(Time, 2) as Time, \
                ROUND(Length / Time * 60, 2)  as CPM FROM Test_Records WHERE filename='%s'\
                order by Time asc limit %c;", filename, no_of_results);
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

int get_average(char *name, int Length)
{
    int rc;
    char *err_msg = 0;
    char *sql = 0;

    /*
     * Check if the test belongs to the default 2finger tests, if it doesn't diregard Length.
     */
    if (strcmp(name,"op") == 0 ||
        strcmp(name,"l;") == 0 ||
        strcmp(name,"./") == 0 ||
        strcmp(name,"qw") == 0 ||
        strcmp(name,"as") == 0 ||
        strcmp(name,"zx") == 0 )
    {
        asprintf(&sql,"SELECT filename, Length, ROUND(AVG(Time), 2) AS [Average Time], \
                ROUND(AVG(Mistakes), 2) AS [Average mistakes per test] FROM Test_Records WHERE filename='%s' \
                AND Length='%d';", name, Length);
    }
    else
    {
        asprintf(&sql,"SELECT filename, Length, ROUND(AVG(Time), 2) AS [Average Time], \
                ROUND(AVG(Mistakes), 2) AS [Average mistakes per test] FROM Test_Records \
                WHERE filename='%s';", name);
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

int get_all_averages(char *filename)
{
    int rc;
    char *err_msg = 0;
    char *sql = 0;
    if (filename)
        asprintf(&sql,"SELECT filename, Length, ROUND(AVG(Time), 2) AS [Average Time], ROUND(AVG(Mistakes), 2) AS\
                [Average mistakes per test],  ROUND(Length / AVG(Time) * 60, 2) as [Clicks per minute], \
                ROUND(AVG(Mistakes) / Length * 100, 2) as [Mistakes per 100 clicks] FROM Test_Records \
                WHERE filename='%s' GROUP BY Length;", filename);
    else
        asprintf(&sql,"SELECT COUNT(filename) as [Tests Taken], Sum(Length) as [Total characters typed], filename, \
                ROUND(Sum(Length) / Sum(Time) * 60, 2) AS [CPM], ROUND(cast(Sum(Mistakes) as FLOAT) /\
                Sum(Length) * 100, 2) AS [Mistakes per 100 key presses] FROM Test_Records GROUP BY filename;");

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

static char* convertRawToText(char *base, uint32_t size)
{
    /* Each byte has a maximum value of 255 which is 3 digits therefore for n bytes we need at most 3 * n bytes. */
    char *result = calloc(size * 3 + 1, 1);
    uint32_t idx = 0;
    for (int i = 0; i < size; i++)
    {
        if (base[i] > 0)
        {
            uint8_t value = base[i];
            uint8_t digits = 0;
            uint8_t tIdx = 0;

            while (value)
            {
                digits++;
                value /= 10;
            }

            /* This will be the idx of the next number. */
            idx += digits;
            /* This points to the last digit of the current number. */
            tIdx = digits - 1;

            value = base[i];

            while (value)
            {
                result[idx - digits + tIdx] = value % 10 + '0';
                value /= 10;
                tIdx--;
            }
        }
        else
        {
            result[idx] = '0';
            idx++;
        }
    }

    return result;
}
