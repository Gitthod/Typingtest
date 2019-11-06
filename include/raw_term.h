#ifndef RAW_TERM_H_123
#define RAW_TERM_H_123

#include <termios.h>
#include <stdint.h>
#include <stdint.h>

/* ------------------------------------------------------------------------------------------------------------------ */
/* ---------------------------------------------------- Defines ----------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

#define TAB_STOP                4
#define ABUF_INIT               {NULL, 0}
#define CTRL_KEY(k)             ((k)   & 0x1f)
#define STATUS_TAB_LINES        1
#define APP_MESSAGE_LINES       3
#define VT100_DEFAULT_TEXT      "\x1b[0m"
#define VT100_CLEAR_CURSOR_DOWN "\x1b[J"
#define VT100_CLEAR_TO_EOL      "\x1b[K"
#define APP_MSG_NEWLINE         VT100_CLEAR_TO_EOL "\r\n"


/* ------------------------------------------------------------------------------------------------------------------ */
/* ------------------------------------------------ Type Definitions ------------------------------------------------ */
/* ------------------------------------------------------------------------------------------------------------------ */

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
    int hlsize;
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
    char appmsg[APP_MESSAGE_LINES * 100];
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

typedef enum termColor
{
    BLACK = 30,
    RED,
    GREEN = 32,
    YELLOW,
    BLUE,
    CYAN,
    LIGHT_BLUE,
    WHITE
} termColor;

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------- Global Function Declarations ---------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */

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
 * Deletes the row with index line and reallocates the rows with higher indexes.
 */
void delRow(int line);

/*
 * Deletes all the rows after line, and moves the cursor to the beginning of the
 * last row.
 */
void delRows(int line);

/*
 * Inserts up to maxLines lines in position line. This function always updates the cursor
 * to be at the next to last row.
 */
int dumpRows(char *string, int maxLines, int line);

/*
 * leaves an error msg point by s, and then exits the program using exit()
 */
void pexit(const char *s);

/*
 * Return the address of the terminal attributes.
 */
termAttributes * getTermAttributes(void);

/*
 * Api to set the application message.
 */
void setAppMessage(const char *fmt, ...);

/*
 * Api to append the application message.
 */
void appendAppMessage(const char *fmt, ...);
/*
 * Hide the custom cursor.
 */
void disableCursor(void);

/*
 * Show the custom cursor.
 */
void enableCursor(void);

/*
 * Color the character at (rowIndex, colIndex) coordinates.
 * The function also will re adjust (and create the first time) the buffer for the character
 * coloring if it is needed. The potential need arises when the row's rsize > hlsize .
 */
void colorPoint(uint32_t rowIndex, uint32_t colIndex, termColor color);

/* Remove the last len character from the specified row. */
void rowTruncateString(tRow *row, uint32_t len);

/* Append string s with the specified length to the end of row. */
void rowAppendString(tRow *row, char *s, uint32_t len);

#endif
