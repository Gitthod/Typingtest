#ifndef RAW_TERM_H_123
#define RAW_TERM_H_123

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/ioctl.h>

/* Type definitios */

enum termKey
{
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/*
 * This is the basic row struct, it holds all the characters of the row
 * the render pointer is basically chars with translated tabs into spaces.
 */
typedef struct tRow
{
    int idx;
    int size;
    int rsize;
    char *chars;
    char *render;
    unsigned char *hl;
} tRow;

/*
 * This struct contains all the information related to our current terminal session.
 * the orig_termios parameter is used to restore the parameters of the terminal before
 * the run of this program.
 */
typedef struct termAttributes
{
    int cx, cy;
    /* rx stand for the position after translating the tabs to spaces */
    int rx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    tRow *row;
    char *statusmsg;
    char appmsg[80];
    struct termios orig_termios;
} termAttributes;

/*
 * This buffer is used to store in a big string all the characters of the
 * visible rows which will at once be written to stdout, to update the screen
 */
typedef struct tBuf
{
    char *b;
    int len;
} tBuf;

/* Defines */

#define TAB_STOP 4
#define ABUF_INIT {NULL, 0}
#define CTRL_KEY(k) ((k) & 0x1f)

/*Function prototypes */

/*
 * Disables some default flags of the teminal to enable us to have
 * more control over it.
 */
void enableRawMode(void);

/*
 * Initializes the termAttributes struct.
 */
termAttributes *initShellAttributes();

/*
 * Reads from stdin the key pressed, in case of an escape sequence, it will
 * translate it to a key specified in termKey enumeration.
 */
int readKey(void);

/*
 * Gets the key returned by readKey() but handles some keys.
 * For now it only handles ctrl+c and ctrl+q so the application
 * can be terminated with them.
 */
int getKey(void);

/*
 * Inserts a character in the position pointed by the cy and cx parameters
 * of the termAttributes struct.
 */
void insertChar(int c);

/*
 * Updates the position of the cursor when pressing arrow keys
 */
void moveCursor(int key);

/*
 * Deletes the row with index line and realocates the rows with higher indexes.
 */
void delRow(int line);

/*
 * Deletes all the rows after line, and moves the curor to the beginning of the
 * last row.
 */
void delRows(int line);

/*
 * Inserts up to maxLine lines in position line. This function always updates the cursor
 * to be at the next to last row.
 */
int dumpRows(char *string, int maxLines, int line);

/*
 * leaves an error msg point by s, and then exits the program using exit()
 */
void pexit(const char *s);

/*
 * Retrun the address of the terminal attributes.
 */
termAttributes * getTermAttributes(void);

/*
 * Api to print some message in the lst line of the terminal.
 */
void setAppMessage(const char *fmt, ...);

/*
 * Hide the custom cursor.
 */
void disableCursor(void);

/*
 * Show the custom cursor.
 */
void enableCursor(void);
#endif

