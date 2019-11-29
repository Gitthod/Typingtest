#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <raw_term.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include "myStrings.h"

/* ------------------------------------------------------------------------------------------------------------------ */
/* ------------------------------------------------ Static Variables ------------------------------------------------ */
/* ------------------------------------------------------------------------------------------------------------------ */

/* Custom struct to control the terminal */
static termAttributes E;
/* Thread to refresh periodically the terminal. */
static pthread_t refreshScreen;
/* Thread to check with low frequency whether the terminal was resized. */
static pthread_t checkResize;
/* Thread to refresh periodically the status bar. */
static pthread_t statusBar;
/* Thread to refresh periodically the application message bar. */
static pthread_t appMessageBar;
/* Thread to refresh periodically the status bar. */
static pthread_t TcustomCursor;
/* When this is set to 0 the thread that refreshes the screen will exit. */
static int th_run = 1;
/* Save some error messages here. */
static char error_messages[300];
/* Is 1 if there was some insert or delete operation. Goes to 0 after the screen is updated. */
static int dirty;
/* Enable or disable the custom cursor.*/
static int show_cursor;

/* Mutex to prevent unsychronized acceses to E static variable*/
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Static Function Declarations ---------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

/*
 * Frees *tB. Could be replaced by free, it only adds readability.
 */
static void tBufFree(tBuf *tB);

/*
 * Append the string pointed by s to tB.
 */
static void tBufAppend(tBuf *tB, const char *s, int len);

/*
 * Get the current cursor position and save it to rows and cols
 */
static int getCursorPosition(int *rows, int *cols);

/*
 * Translate \t to spaces according to the TAB_STOP length.
 */
static int translateTabs(tRow *row, int cx);

/*
 * Get the current windows size of the terminal and save it to rows and cols.
 */
static int getWindowSize(int *rows, int *cols);

/*
 * Return the terminal to its original settings before enableRawMode().
 */
static void disableRawMode(void);
/*
 * Readujst the offsets so you can scroll to more rows than your terminal size allows.
 */
static void shellScroll(void);

/*
 * Function that refreshes the terminal with the news rows and app message.
 */
static void refreshTerminal(void);

/*
 * Appends every row to the buffer so the can be later written to stdout at once.
 */
static void drawRows(tBuf *tB);

/*
 * Get the current time and print it in the previous to last row using escape codes.
 */
static void printStatusMessage(void);

/*
 * This function is meant to be used as a thread to periodically update the status message.
 */
static void updateStatusMessage(void);

/*
 * Update the specified row to have its spaces recalculated after tab insertions.
 */
static void updateRow(tRow *row);

/*
 * Free the memory occupied by row.
 */
static void freeRow(tRow *row);

/*
 * Insert a new row with size len from point s, at index line.
 */
static void insertRow(int line, char *s, size_t len);

/*
 * Checks a static variable if it's different than 0 to continue calling refreshTerminal().
 * This function is run in a separate thread.
 */
static void keepRefresing(void);

/*
 * Checks the dimensions of the terminal to readjust it in case of a resize.
 */
static void adjustNewDimensions(void);

/*
 * This function will print a custom symbol as a cursor, it is meant to be used in a thread.
 */
static void customCursor(void);

/*
 * Prints the application message.
 */
static void printAppMessage(void);

/*
 * This functions prints the application message, it should be used in a thread.
 */
static void updateAppMessage(void);

/*
 * Abstracts character insertion in one row.
 */
static void rowInsertChar(tRow *row, int line, int c);

/* ------------------------------------------------------------------------------------------------------------------ */
/* ----------------------------------------- Static Function Implementations ---------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

static void keepRefresing(void)
{
    while (th_run)
    {
        pthread_mutex_lock(&mutex);

        if (dirty)
            refreshTerminal();

        pthread_mutex_unlock(&mutex);

        /* Wait two milliseconds to avoid unnecessary load. */
        usleep(2000);
    }
}

static void rowInsertChar(tRow *row, int offset, int c)
{
    pthread_mutex_lock(&mutex);
    if (offset < 0 || offset > row->size)
        offset = row->size;

    /* row->chars always comes from malloc */
    row->chars = (char *)realloc(row->chars, row->size + 2);

    if (row->chars == 0)
        pexit("rowAppendString");

    memmove(&row->chars[offset + 1], &row->chars[offset], row->size - offset + 1);
    row->size++;
    row->chars[offset] = c;
    updateRow(row);
    pthread_mutex_unlock(&mutex);
}

static void updateStatusMessage(void)
{
    while (th_run)
    {
        /* The following check serves to reduce resources. */
        struct tm *timeinfo;
        time_t currentTime= time(NULL);
        static int prev_seconds = - 1;
        int update = 0;
        timeinfo = localtime (&currentTime);
        if (timeinfo->tm_sec != prev_seconds)
        {
            update = 1;
            prev_seconds = timeinfo->tm_sec;
        }
        if (update)
        {
            /* This thread has to be locked because it changes the cursor position. */
            pthread_mutex_lock(&mutex);
            printStatusMessage();
            pthread_mutex_unlock(&mutex);
        }

        usleep(100000);
    }
}

static void refreshTerminal()
{
    dirty = 0;
    shellScroll();

    tBuf tB = ABUF_INIT;

    tBufAppend(&tB, "\x1b[H", 3);

    drawRows(&tB);

    char buf[32];
    /* Move the cursor to the position indicated by E.cx and E.cy subtracting their respective offsets. */
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);

    tBufAppend(&tB, buf, strlen(buf));

    write(STDOUT_FILENO, tB.b, tB.len);
    tBufFree(&tB);
}

static void freeRow(tRow *row)
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}

static void updateRow(tRow *row)
{
    dirty = 1;
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t')
            tabs++;

    free(row->render);
    row->render = (char *)malloc(row->size + tabs*(TAB_STOP - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            while (idx % TAB_STOP != 0)
                row->render[idx++] = ' ';
        }
        else
        {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

static void printStatusMessage(void)
{
    char *message;
    /* Move the cursor to the line after the test's last row. */
    asprintf(&message, "\x1b[%d;%dH", E.screenrows + 1, 0);
    write(STDOUT_FILENO, message, strlen(message));
    free(message);

    /* Get the current time and print it in that line. */
    struct tm * timeinfo;
    time_t currentTime= time(NULL);
    timeinfo = localtime (&currentTime);
    asprintf(&E.statusmsg, "\x1b[7mCurrent time : %s\x1b[m", asctime(timeinfo));
    asprintf(&message, "\x1b[K%s",E.statusmsg);
    write(STDOUT_FILENO, message, strlen(message));
    free(message);
    free(E.statusmsg);

    /* Return the cursor to the original position. */
    asprintf(&message, "\x1b[%d;%dH",(E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    write(STDOUT_FILENO, message, strlen(message));
    free(message);
}

static void printAppMessage(void)
{
    char *message;
    /* Move the cursor to the line after the test's last row. */
    asprintf(&message, "\x1b[%d;%dH", E.screenrows + 2, 0);
    write(STDOUT_FILENO, message, strlen(message));
    free(message);

    asprintf(&message, "%s"
             VT100_DEFAULT_TEXT
             VT100_CLEAR_CURSOR_DOWN, E.appmsg);
    write(STDOUT_FILENO, message, strlen(message));
    free(message);

    /* Return the cursor to the original position. */
    asprintf(&message, "\x1b[%d;%dH",(E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    write(STDOUT_FILENO, message, strlen(message));
    free(message);
}

static void updateAppMessage(void)
{
    while (th_run)
    {
        /* This functions has to be locked because it changes the cursor position. */
        pthread_mutex_lock(&mutex);
        printAppMessage();
        pthread_mutex_unlock(&mutex);

        usleep(100000);
    }
}

static void adjustNewDimensions(void)
{
    while (th_run)
    {
        struct winsize ws;

        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
        if ((ws.ws_row - (STATUS_TAB_LINES + APP_MESSAGE_LINES)) != E.screenrows || ws.ws_col != E.screencols)
        {
            pthread_mutex_lock(&mutex);

            E.screencols = ws.ws_col;
            E.screenrows = ws.ws_row - (STATUS_TAB_LINES + APP_MESSAGE_LINES);
            write(STDOUT_FILENO,"\x1b[2J", 4);
            /* Apparently when the terminal is resized the cursors appears again so it needs to be hidden again. */
            write(STDOUT_FILENO,"\x1b[?25l", 6);

            refreshTerminal();
            printStatusMessage();
            printAppMessage();

            pthread_mutex_unlock(&mutex);
        }

        /* Wait one tenth of a second to save resources */
        usleep(100000);
    }
}

static void customCursor(void)
{
    while (th_run)
    {
        if (show_cursor)
        {
            char *message;

            pthread_mutex_lock(&mutex);
            asprintf(&message, "\x1b[%d;%dH",(E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
            write(STDOUT_FILENO, message, strlen(message));
            write(STDOUT_FILENO, "\u2588", 3);
            pthread_mutex_unlock(&mutex);

            free(message);
        }
        usleep(10000);
    }
    pthread_exit(NULL);
}

void colorPoint(uint32_t rowIndex, uint32_t colIndex, termColor color)
{

    /* Fail silently if the parameters are wrong but print to stderr. */
    if (rowIndex >= E.numrows ||
        colIndex > E.row[rowIndex].size)
    {
        fputs("colorRow called with out of bound indexes\n", stderr);
        return;
    }

    pthread_mutex_lock(&mutex);

    /*
     * It's not needed to realloc if rsize is smaller since it would
     * consume a lot of processing power for insignificant memory.
     */
    if (E.row[rowIndex].rsize > E.row[rowIndex].hlsize)
    {
        int diff = E.row[rowIndex].rsize - E.row[rowIndex].hlsize;
        E.row[rowIndex].hl = realloc(E.row[rowIndex].hl, E.row[rowIndex].rsize);

        if ( diff > 0)
        {
            /* Initialize with zeros the new elements. */
            for (int i = 0; i < diff; i++)
                E.row[rowIndex].hl[E.row[rowIndex].hlsize + i] = 0;

            E.row[rowIndex].hlsize = E.row[rowIndex].rsize;
        }
    }

    /* Translate colIndex to renderedIndex. */
    uint32_t renderedIndex = 0;
    for (int i = 0; i < colIndex; i++)
        if (E.row[rowIndex].chars[i] == '\t')
            renderedIndex = (renderedIndex / TAB_STOP + 1 ) * TAB_STOP;
        else
            renderedIndex++;

    E.row[rowIndex].hl[renderedIndex - 1] = color;
    dirty = 1;

    pthread_mutex_unlock(&mutex);
}

static void tBufFree(tBuf *tB)
{
    free(tB->b);
}

static int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;

        if (buf[i] == 'R')
            break;

        i++;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;

    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;

    return 0;
}

static int getWindowSize(int *rows, int *cols)
{
    /*Move the Cursor to the bottom right corner */
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
        return -1;

    return getCursorPosition(rows, cols);
}

static void disableRawMode(void)
{
    th_run = 0;
    pthread_join(refreshScreen, NULL);
    pthread_join(checkResize, NULL);
    pthread_join(statusBar, NULL);
    pthread_join(appMessageBar, NULL);
    pthread_join(TcustomCursor, NULL);
    write(STDOUT_FILENO,"\x1b[H\x1b[J", 6);

    /* Re-enable the cursor. */
    write(STDOUT_FILENO,"\x1b[?25h", 6);
    printf("%s\r", error_messages);
    delRows(0);
    free(E.row);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        pexit("tcsetattr");
}

static int translateTabs(tRow *row, int cx)
{
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++)
    {
        if (row->chars[j] == '\t')
            rx += (TAB_STOP - 1) - (rx % TAB_STOP);

        rx++;
    }
    return rx;
}

/*
 * This function automatically adjustes the rowoff and coloff based on cx and cy positions so we can properly refresh
 * the terminal.
 */
static void shellScroll(void)
{
    E.rx = 0;
    if (E.cy < E.numrows)
        E.rx = translateTabs(&E.row[E.cy], E.cx);

    if (E.cy < E.rowoff)
        E.rowoff = E.cy;

    if (E.cy >= E.rowoff + E.screenrows)
        E.rowoff = E.cy - E.screenrows + 1;

    if (E.rx < E.coloff)
        E.coloff = E.rx;

    if (E.rx >= E.coloff + E.screencols)
        E.coloff = E.rx - E.screencols + 1;
}

static void drawRows(tBuf *tB)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        int filtRow = y + E.rowoff;
        if (filtRow >= E.numrows)
        {
            tBufAppend(tB, "~", 1);
        }
        else
        {
            int len = E.row[filtRow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols)
                len = E.screencols;

            char *c = &E.row[filtRow].render[E.coloff];
            int j;
            for (j = 0; j < len; j++)
            {
                if (iscntrl(c[j]) && c[j] != '\x1b')
                {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    tBufAppend(tB, "\x1b[7m", 4);
                    tBufAppend(tB, &sym, 1);
                    tBufAppend(tB, "\x1b[m", 3);
                }
                else
                {
                    if ( !E.row[filtRow].hl || !E.row[filtRow].hl[j])
                        tBufAppend(tB, &c[j], 1);
                    else
                    {
                        char tempColorCode[6];
                        tempColorCode[5] = 0;
                        /* This supposes that %hhd is always 2 digits long. */
                        snprintf(tempColorCode, 6, "\x1b[%dm", E.row[filtRow].hl[j]);

                        tBufAppend(tB, tempColorCode, 5);
                        tBufAppend(tB, &c[j], 1);
                        tBufAppend(tB, "\x1b[0m", 4);
                    }
                }
            }
        }
        tBufAppend(tB, "\x1b[K", 3);
        tBufAppend(tB, "\r\n", 2);
    }
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* ----------------------------------------- Global Function Implementations ---------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

void pexit(const char *s)
{
    int idx = 0;
    /*
     * Write a string to stderr  and also copy to the static array for errors.
     * Writing to stderr allows to save errors by redirection
     * For example <prog_name> <args ..> 2 >> errors
     */
    fputs(s, stderr);
    while ((error_messages[idx++] = *s++));

    exit(1);
}

termAttributes * initShellAttributes(void)
{
    pthread_mutex_lock(&mutex);
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    /* Unused but functional */
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.statusmsg = NULL;
    E.appmsg[0] = '\0';

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        pexit("getWindowSize");
    /* Save one row for the status and three for the Message bar */
    E.screenrows -= STATUS_TAB_LINES + APP_MESSAGE_LINES;
    pthread_mutex_unlock(&mutex);

    return &E;
}

void tBufAppend(tBuf *tB, const char *s, int len)
{
    char *new = (char *)realloc(tB->b, tB->len + len);

    if (new == 0)
        pexit("tBufAppend");

    memcpy(&new[tB->len], s, len);
    tB->b = new;
    tB->len += len;
}

void enableRawMode(void)
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        pexit("tcgetattr");
    /* Restore original paremeters on exit. */
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* Hide the cursor. */
    write(STDOUT_FILENO,"\x1b[?25l", 6);
    /*
     * Since we use raw mode we we periodically refresh the screen using a thred
     */
    pthread_create(&refreshScreen, NULL, (void *(*)(void *))keepRefresing, NULL);
    pthread_create(&statusBar, NULL, (void *(*)(void *))updateStatusMessage, NULL);
    pthread_create(&appMessageBar, NULL, (void *(*)(void *))updateAppMessage, NULL);
    pthread_create(&TcustomCursor, NULL, (void *(*)(void *))customCursor, NULL);
    pthread_create(&checkResize, NULL, (void *(*)(void *))adjustNewDimensions, NULL);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        pexit("tcsetattr");
}

void setAppMessage(const char *fmt, ...)
{
    pthread_mutex_lock(&mutex);
    /* clear the rest of the line before you go to the new line, also clear the end of the last line. */

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.appmsg, sizeof(E.appmsg), fmt, ap);

    {
        char *processedMessage = substitutePattern(E.appmsg, "\n", APP_MSG_NEWLINE);
        int i = 0;
        for(; processedMessage[i]; i++)
            E.appmsg[i] = processedMessage[i];

        E.appmsg[i] = 0;
        free(processedMessage);
    }

    va_end(ap);
    pthread_mutex_unlock(&mutex);
}

void appendAppMessage(const char *fmt, ...)
{
    pthread_mutex_lock(&mutex);
    int msgLength = strlen(E.appmsg);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.appmsg + msgLength, sizeof(E.appmsg) - msgLength, fmt, ap);
    va_end(ap);
    pthread_mutex_unlock(&mutex);
}

void delRow(int line)
{
    pthread_mutex_lock(&mutex);
    dirty = 1;
    if (line < 0 || line >= E.numrows)
    {
        pthread_mutex_unlock(&mutex);
        return;
    }

    freeRow(&E.row[line]);

    memmove(&E.row[line], &E.row[line + 1], sizeof(tRow) * (E.numrows - line - 1));

    for (int j = line; j < E.numrows - 1; j++)
        E.row[j].idx--;

    /* Move the cursor if it's ahead of the delete line. */
    if (E.cy > line )
        E.cy--;

    E.numrows--;
    if (E.numrows < E.screenrows)
        E.rowoff = 0;
    else
        E.rowoff = E.numrows - E.screenrows;
    pthread_mutex_unlock(&mutex);
}

void delRows(int line)
{
    while (line < E.numrows)
        delRow(line);
}

void moveCursor(int key)
{
    pthread_mutex_lock(&mutex);
    dirty = 1;
    tRow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key)
    {
        case ARROW_LEFT:
            if (E.cx != 0)
            {
                E.cx--;
            }
            else if (E.cy > 0)
            {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;

        case ARROW_RIGHT:
            if (row && E.cx < row->size)
            {
                E.cx++;
            }
            else if (row && E.cx == row->size) {
                E.cy++;
                E.cx = 0;
            }
            break;

        case ARROW_UP:
            if (E.cy != 0)
                E.cy--;

            break;

        case ARROW_DOWN:
            if (E.cy < E.numrows)
                E.cy++;

            break;
    }

    row = (E.cy >= E.numrows) ?  NULL : &E.row[E.cy];

    int rowlen = row ?  row->size : 0;
    if (E.cx > rowlen)
        E.cx = rowlen;

    pthread_mutex_unlock(&mutex);
}

int readKey()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            pexit("read");
    }

    if (c == '\x1b')
    {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';
        if (seq[0] == '[')
        {
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else
            {
                switch (seq[1])
                {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O')
        {
            switch (seq[1])
            {
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
            }
        }
        return '\x1b';
    }
    else
    {
        return c;
    }
}

void rowAppendString(tRow *row, char *s, uint32_t len)
{
    pthread_mutex_lock(&mutex);
    /*Check len is valid*/
    for (int i = 0; i < len; i++)
        if (!s[i])
            pexit("rowAppendString");

    row->chars = (char *)realloc(row->chars, row->size + len + 1);

    if (row->chars == 0)
        pexit("rowAppendString");

    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';

    updateRow(row);
    pthread_mutex_unlock(&mutex);
}

void rowTruncateString(tRow *row, uint32_t len)
{
    pthread_mutex_lock(&mutex);

    if (len > row->size)
        pexit("rowTruncateString");

    row->chars = (char *)realloc(row->chars, row->size - len + 1);
    row->size -= len;

    if (row->chars == 0)
        pexit("rowTruncateString");

    row->chars[row->size] = '\0';

    updateRow(row);
    pthread_mutex_unlock(&mutex);
}

int getKey()
{
    int c = readKey();

    switch (c)
    {
        case CTRL_KEY('q'):
            th_run = 0;
            exit(0);
            break;

        case CTRL_KEY('c'):
            th_run = 0;
            exit(0);
            break;

        default:
            return c;
            break;
    }
}

void insertChar(int c)
{
    pthread_mutex_lock(&mutex);
    if (E.cy == E.numrows)
    {
        pthread_mutex_unlock(&mutex);
        insertRow(E.numrows, "", 0);
    }

    pthread_mutex_unlock(&mutex);

    rowInsertChar(&E.row[E.cy], E.cx, c);

    pthread_mutex_lock(&mutex);

    E.cx++;
    if (E.cx == E.screencols - 1)
    {
        E.cy = E.numrows;
        E.cx = 0;
        pthread_mutex_unlock(&mutex);
        insertRow(E.numrows, "", 0);
    }
    pthread_mutex_unlock(&mutex);
}


void insertRow(int line, char *s, size_t len)
{
    pthread_mutex_lock(&mutex);
    if (line < 0 || line > E.numrows)
    {
        pthread_mutex_unlock(&mutex);
        return;
    }

    E.row = (tRow *)realloc(E.row, sizeof(tRow) * (E.numrows + 1));

    if (E.row == 0)
        pexit("insertRow");

    memmove(&E.row[line + 1], &E.row[line], sizeof(tRow) * (E.numrows - line));
    for (int j = line + 1; j <= E.numrows; j++)
        E.row[j].idx++;

    /* Initialize the new line */
    {
        E.row[line].idx = line;
        E.row[line].size = len;
        E.row[line].chars = (char *)malloc(len + 1);
        E.row[line].chars[len] = '\0';
        E.row[line].rsize = 0;
        E.row[line].hlsize = 0;
        E.row[line].render = NULL;
        E.row[line].hl = NULL;
    }

    memcpy(E.row[line].chars, s, len);

    updateRow(&E.row[line]);

    E.numrows++;
    pthread_mutex_unlock(&mutex);
}

/*
 * Inserts up to maxLine lines in position line. This function always updates the cursor
 * to be at the next to last row.
 */
int dumpRows(char *string, int maxLines, int line)
{
    int idx = 0;
    int len = 0;
    int rowsCopied = 0;

    if (line < 0 || line > E.numrows)
        return -1;

    while (string[idx])
    {
        if (string[idx] == '\n')
        {
            insertRow(line + rowsCopied++, &string[idx++] - len, len);
            len = 0;

            if (maxLines == rowsCopied)
            {
                pthread_mutex_lock(&mutex);

                E.cy = E.numrows;
                E.cx = 0;

                pthread_mutex_unlock(&mutex);

                return idx;
            }
        }
        else
        {
            if (len == E.screencols - 2)
            {
                insertRow(line + rowsCopied, &string[idx++] - len, len + 1);
                len = 0;
                rowsCopied++;

                if (maxLines == rowsCopied)
                {
                    pthread_mutex_lock(&mutex);

                    E.cy = E.numrows;
                    E.cx = 0;

                    pthread_mutex_unlock(&mutex);

                    return idx;
                }
            }
            else
            {
                len++;
                idx++;
            }
        }
    }

    if (len)
    {
        insertRow(line + rowsCopied++, &string[idx] - len, len);
    }

    /*This ensures that an exact number of lines will be dumped if the number isn't 0. */
    for (int i = rowsCopied; i < maxLines; i++, rowsCopied++)
        insertRow(line + i, "", 0);

    pthread_mutex_lock(&mutex);

    E.cy = E.numrows;
    E.cx = 0;

    pthread_mutex_unlock(&mutex);

    return idx;
}


termAttributes * getTermAttributes(void)
{
    return &E;
}

void enableCursor(void)
{
    show_cursor = 1;
}

void disableCursor(void)
{
    show_cursor = 0;
}
