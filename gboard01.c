#include <stdio.h>     // Standard input / output
#include <stdlib.h>    // Standard library
#include <string.h>    // String handling
#include <unistd.h>    // OS API
#include <ncurses.h>   // Character cell display

// Structure to store the game board
typedef struct {
    int width;
    int height;
    char *cells;       // Pointer to the string
} GameBoard;

// Function to create a gameboard with the required dimensions
GameBoard *createBoard(int width, int height) {
    GameBoard *board = malloc(sizeof(GameBoard));  // Gets a pointer to the board
    board->width = width;                          // Set the board width
    board->height = height;                        // Set the board height
    board->cells = malloc(width * height + 1);     // Allocate memory for the board data + 1
    memset(board->cells, '0', width * height);     // Fill the board data with zeros
    board->cells[width * height] = '\0';           // Stick a NULL character at the end of the data
    return board;                                  // Return the board item
}

// Free up the memory used by the board
void freeBoard(GameBoard *board) {
    free(board->cells);
    free(board);
}

// Set a given cell to the value passed in
void setCell(GameBoard *board, int x, int y, char value) {
    if (x >= 0 && x < board->width && y >= 0 && y < board->height) {
        board->cells[y * board->width + x] = value;
    }
}

// Return the value of a given cell, return '?' if the coordinated fall outside the board
char getCell(GameBoard *board, int x, int y) {
    if (x >= 0 && x < board->width && y >= 0 && y < board->height) {
        return board->cells[y * board->width + x];
    }
    return '?';
}

// Display the board on the screen
void printBoard(GameBoard *board) {
    clear();
    for (int y = 0; y < board->height; y++) {
        for (int x = 0; x < board->width; x++) {
            char c = board->cells[y * board->width + x];
            if (c == '0') {
                mvaddch(y, x, '.'); // empty
            } else {
                mvaddch(y, x, '#'); // filled
            }
        }
    }
    refresh();
}

// ********************************************************************
// Mainline routine
// ********************************************************************
int main() {
    // Set the width and height and create the board
    int width = 8, height = 16;
    GameBoard *board = createBoard(width, height);

    initscr();            // start ncurses
    noecho();             // don’t echo typed chars
    curs_set(FALSE);      // hide cursor
    keypad(stdscr, TRUE); // enable arrow keys
    nodelay(stdscr, TRUE);// non-blocking input

    // Start the game loop
    while (1) {

        int x = width / 2;
        int y = 0;
        int falling = 1;

        while (falling) {
            // draw current board + falling cell
            printBoard(board);
            mvaddch(y, x, '*'); // falling piece
            refresh();

            // handle input
            int ch = getch();
            if (ch == KEY_LEFT && x > 0 && getCell(board, x - 1, y) == '0') {
                x--;
            } else if (ch == KEY_RIGHT && x < width - 1 && getCell(board, x + 1, y) == '0') {
                x++;
            }

            // check collision
            if (y == height - 1 || getCell(board, x, y + 1) != '0') {
                setCell(board, x, y, '1'); // lock piece
                falling = 0;
            } else {
                y++;
            }

            usleep(200000); // delay (200ms)
        }

        // short pause before next piece
        usleep(500000);
    }

    endwin();
    freeBoard(board);
    return 0;
}

