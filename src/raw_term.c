#include <raw_term.h>

/* Local variables */
/* Custom struct to control the terminal */
static termAttributes E;
/* Thread to refresh periodically the terminal. */
static pthread_t refreshScreen;
/* When this is set to 0 the thread that refreshes the screen will exit. */
static int th_run = 1;
/* Save some error messages here. */
static char error_messages[300];

/* Mutex to prevent unsychronized acceses to E static variable*/
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Local functions */
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
 * Function that refreshes the terminal with the news rows and status and app message.
 */
static void refreshTerminal(void);

/*
 * Appends every row to the buffer so the can be later written to stdout at once.
 */
static void drawRows(tBuf *tB);

/*
 * Append the status bar to the tB buffer, this will normally be called after drawRows. So it occupies the next line.
 */
static void drawStatusBar(tBuf *tB);

/*
 * Append the app message to the tB buffer, this will normally be called after drawStatusBar. So it occupies the next
 * line.
 */
static void drawAppMessage(tBuf *tb);

/*
 * Set the status message which is defined in the struct termAttributes to a formatted string.
 */
static void setStatusMessage(const char *fmt, ...);

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

void pexit(const char *s)
{
    int idx = 0;
    /* Write string to stderr  and also copy to the static array for errors. */
    perror(s);
    while((error_messages[idx++] = *s++));

    exit(1);
}

static void tBufFree(tBuf *tB)
{
    free(tB->b);
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
    E.statusmsg[0] = '\0';
    E.appmsg[0] = '\0';
    E.statusmsg_time = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        pexit("getWindowSize");
    /* Save one row for the status and one for the Message bar */
    E.screenrows -= 2;
    pthread_mutex_unlock(&mutex);

    return &E;
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


void tBufAppend(tBuf *tB, const char *s, int len)
{
    char *new = (char *)realloc(tB->b, tB->len + len);

    if (new == 0)
        pexit("tBufAppend");

    memcpy(&new[tB->len], s, len);
    tB->b = new;
    tB->len += len;
}


static void disableRawMode(void)
{
    th_run = 0;
    pthread_join(refreshScreen, NULL);
    write(STDOUT_FILENO,"\x1b[H\x1b[J", 6);
    printf("%s\r", error_messages);
    delRows(0);
    free(E.row);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        pexit("tcsetattr");

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

    /*
     * Since we use raw mode we we periodically refresh the screen using a thred
     */
    pthread_create(&refreshScreen, NULL, (void *(*)(void *))keepRefresing, NULL);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
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
                if (iscntrl(c[j]))
                {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    tBufAppend(tB, "\x1b[7m", 4);
                    tBufAppend(tB, &sym, 1);
                    tBufAppend(tB, "\x1b[m", 3);
                }
                else
                {
                    tBufAppend(tB, &c[j], 1);
                }

            }
        }
        tBufAppend(tB, "\x1b[K", 3);
        tBufAppend(tB, "\r\n", 2);
    }
}

static void drawStatusBar(tBuf *tB)
{
    tBufAppend(tB, "\x1b[7m", 4);

    /* Ensure there are no newline characters in the status msg */
    for(int i = 0; i < sizeof(E.statusmsg); i++)
        if (E.statusmsg[i] == '\n')
        {
            E.statusmsg[i] = 0;
            break;
        }

    tBufAppend(tB, E.statusmsg, strlen(E.statusmsg));

    tBufAppend(tB, "\x1b[m", 3);
    tBufAppend(tB, "\x1b[K", 3);
    tBufAppend(tB, "\r\n", 2);
}

static void drawAppMessage(tBuf *tB)
{
    /* Ensure there are no newline characters in the app msg */
    for(int i = 0; i < sizeof(E.appmsg); i++)
        if (E.appmsg[i] == '\n')
        {
            E.appmsg[i] = 0;
            break;
        }

    /* If sizeof is used instead of strlen the message will be merged with previous. */
    tBufAppend(tB, E.appmsg, strlen(E.appmsg));
    if(E.appmsg[0])
        tBufAppend(tB, "\x1b[39m", 5);
    tBufAppend(tB, "\x1b[K", 3);
}

void setAppMessage(const char *fmt, ...)
{
    pthread_mutex_lock(&mutex);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.appmsg, sizeof(E.appmsg), fmt, ap);
    va_end(ap);
    pthread_mutex_unlock(&mutex);
}

static void setStatusMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    /* time(0) returns the time, instead of passing it to the parameter */
    E.statusmsg_time = time(NULL);
}



static void refreshTerminal()
{
    shellScroll();

    tBuf tB = ABUF_INIT;

    tBufAppend(&tB, "\x1b[?25l", 6);
    tBufAppend(&tB, "\x1b[H", 3);
    struct tm * timeinfo;
    timeinfo = localtime (&E.statusmsg_time);

    drawRows(&tB);
    setStatusMessage("Current time : %s", asctime(timeinfo));
    drawStatusBar(&tB);
    drawAppMessage(&tB);


    char buf[32];
    /* Move the cursor to the position indicated by E.cx and E.cy subtracting their respective offsets. */
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);

    tBufAppend(&tB, buf, strlen(buf));
    tBufAppend(&tB, "\x1b[?25h", 6);

    write(STDOUT_FILENO, tB.b, tB.len);
    tBufFree(&tB);
}

void delRow(int line)
{
    pthread_mutex_lock(&mutex);
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
    pthread_mutex_unlock(&mutex);
}

void delRows(int line)
{
    pthread_mutex_lock(&mutex);
    if (line < 0 || line >= E.numrows)
    {
        pthread_mutex_unlock(&mutex);
        /* Even if the line doesn't exist update the cursor. */
        E.cy = E.numrows - E.rowoff;
        E.cx = 0;
        return;
    }

    for (int i = line; i < E.numrows; i++)
        freeRow(&E.row[i]);

    E.numrows = line;
    if (E.numrows < E.screenrows)
        E.rowoff = 0;
    else
        E.rowoff = E.numrows - E.screenrows;

    E.cy = E.numrows - E.rowoff;
    E.cx = 0;
    pthread_mutex_unlock(&mutex);
}

static void freeRow(tRow *row)
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void moveCursor(int key)
{
    pthread_mutex_lock(&mutex);
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

static void updateRow(tRow *row)
{
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

void rowAppendString(tRow *row, char *s, size_t len)
{
    /*Check len is valid*/
    for(int i = 0; i < len; i++)
        if(!s[i])
            pexit("rowAppendString");

    row->chars = (char *)realloc(row->chars, row->size + len + 1);

    if (row->chars == 0)
        pexit("rowAppendString");

    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';

    updateRow(row);
}

int getKey()
{
    int c = readKey();

    switch (c)
    {
        case CTRL_KEY('q'):
            th_run = 0;
            pthread_join(refreshScreen, NULL);
            exit(0);
            break;

        case CTRL_KEY('c'):
            th_run = 0;
            pthread_join(refreshScreen, NULL);
            exit(0);
            break;

        default:
            return c;
            break;
    }
}

void rowInsertChar(tRow *row, int line, int c)
{
    if (line < 0 || line > row->size)
        line = row->size;

    /* row->chars always comes from malloc */
    row->chars = (char *)realloc(row->chars, row->size + 2);

    if (row->chars == 0)
        pexit("rowAppendString");

    memmove(&row->chars[line + 1], &row->chars[line], row->size - line + 1);
    row->size++;
    row->chars[line] = c;
    updateRow(row);
}

void insertChar(int c)
{
    pthread_mutex_lock(&mutex);
    if (E.cy == E.numrows)
    {
        pthread_mutex_unlock(&mutex);
        insertRow(E.numrows, "", 0);
    }

    rowInsertChar(&E.row[E.cy], E.cx, c);

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

    E.row[line].idx = line;

    E.row[line].size = len;
    E.row[line].chars = (char*)malloc(len + 1);
    memcpy(E.row[line].chars, s, len);
    E.row[line].chars[len] = '\0';
    E.row[line].rsize = 0;

    E.row[line].render = NULL;
    E.row[line].hl = NULL;
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

    while(string[idx])
    {
        if(string[idx] == '\n')
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

    if(len)
    {
        insertRow(line + rowsCopied++, &string[idx] - len, len);
    }

    /*This ensures that an exact number of lines will be dumped if the number isn't 0. */
    for(int i = rowsCopied; i < maxLines; i++, rowsCopied++)
        insertRow(line + i, "", 0);

    pthread_mutex_lock(&mutex);

    E.cy = E.numrows;
    E.cx = 0;

    pthread_mutex_unlock(&mutex);

    return idx;
}

static void keepRefresing(void)
{
    while (th_run)
    {
        pthread_mutex_lock(&mutex);
        refreshTerminal();
        pthread_mutex_unlock(&mutex);

        /* Wait two milliseconds to avoid unnecessary load. */
        usleep(2000);
    }
}

termAttributes * getTermAttributes(void)
{
    return &E;
}
