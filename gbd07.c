#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <time.h>

typedef struct
{
    int width;
    int height;
    char *cells;
} GameBoard;

GameBoard *createBoard(int width, int height)
{
    GameBoard *board = malloc(sizeof(GameBoard));
    board->width = width;
    board->height = height;
    board->cells = malloc(width * height);
    memset(board->cells, '0', width * height);
    return board;
}

// ****************************************************************************
// Free up the memory used by the board.
// ****************************************************************************
void freeBoard(GameBoard *board)
{
    free(board->cells);
    free(board);
}

// ****************************************************************************
// Set the value of a given cell.
// ****************************************************************************
void setCell(GameBoard *board, int x, int y, char value)
{
    if (x >= 0 && x < board->width && y >= 0 && y < board->height)
    {
        board->cells[y * board->width + x] = value;
    }
}

// ****************************************************************************
// Return the value of a given cell.
// ****************************************************************************
char getCell(GameBoard *board, int x, int y)
{
    if (x >= 0 && x < board->width && y >= 0 && y < board->height)
    {
        return board->cells[y * board->width + x];
    }
    return '?';
}

// ****************************************************************************
// Display the gameboard
// ****************************************************************************
void printBoard(GameBoard *board, int score, int elapsed)
{
    clear();
    for (int y = 0; y < board->height; y++)
    {
        for (int x = 0; x < board->width; x++)
        {
            char c = board->cells[y * board->width + x];
            mvaddch(y, x, (c == '0') ? '.' : '#');
        }
    }
    mvprintw(board->height + 1, 0, "Score: %d", score);
    mvprintw(board->height + 2, 0, "Time: %d seconds", elapsed);
    refresh();
}

typedef struct
{
    int x[4];
    int y[4];
} Shape;

Shape tetrominoes[7] = {
    {{0, 1, 2, 3}, {0, 0, 0, 0}}, // I
    {{0, 1, 0, 1}, {0, 0, 1, 1}}, // O
    {{0, 1, 2, 1}, {0, 0, 0, 1}}, // T
    {{1, 2, 0, 1}, {0, 0, 1, 1}}, // S
    {{0, 1, 1, 2}, {0, 0, 1, 1}}, // Z
    {{0, 0, 1, 2}, {0, 1, 1, 1}}, // J
    {{2, 0, 1, 2}, {0, 1, 1, 1}}  // L
};

// ****************************************************************************
// Rotate the given shape 90 degrees clockwise around its origin.
// For novice readers: this changes each block's x,y coordinates,
// then shifts the shape so its minimum x and y start at 0.
// ****************************************************************************   
void rotateClockwise(Shape *s)
{
    for (int i = 0; i < 4; i++)
    {
        int oldX = s->x[i], oldY = s->y[i];
        s->x[i] = oldY;
        s->y[i] = -oldX;
    }
    int minX = s->x[0], minY = s->y[0];
    for (int i = 1; i < 4; i++)
    {
        if (s->x[i] < minX)
            minX = s->x[i];
        if (s->y[i] < minY)
            minY = s->y[i];
    }
    for (int i = 0; i < 4; i++)
    {
        s->x[i] -= minX;
        s->y[i] -= minY;
    }
}

// ****************************************************************************
// Check if placing shape s at offset (ox,oy) would collide.
// Returns 1 if there is a collision (out of bounds or overlapping blocks),
// 0 if the placement is clear. Useful to prevent illegal moves. 
// ****************************************************************************
int collision(GameBoard *board, Shape *s, int ox, int oy)
{
    for (int i = 0; i < 4; i++)
    {
        int bx = ox + s->x[i];
        int by = oy + s->y[i];
        if (bx < 0 || bx >= board->width || by >= board->height)
            return 1;
        if (by >= 0 && getCell(board, bx, by) != '0')
            return 1;
    }
    return 0;
}

// ****************************************************************************
// Fix the current shape into the board cells permanently.
// This writes the shape's blocks into the board so they become part of the playfield.
// ****************************************************************************
void lockShape(GameBoard *board, Shape *s, int ox, int oy)
{
    for (int i = 0; i < 4; i++)
    {
        int bx = ox + s->x[i];
        int by = oy + s->y[i];
        if (bx >= 0 && bx < board->width && by >= 0 && by < board->height)
        {
            setCell(board, bx, by, '1');
        }
    }
}

// ****************************************************************************
// Scan the board for full horizontal lines, remove them, shift everything above down,
// and return how many lines were cleared. Also shows a simple clear animation.
// ****************************************************************************
int clearLines(GameBoard *board)
{
    int cleared = 0;
    for (int y = 0; y < board->height; y++)
    {
        int full = 1;
        for (int x = 0; x < board->width; x++)
        {
            if (getCell(board, x, y) == '0')
            {
                full = 0;
                break;
            }
        }
        if (full)
        {
            for (int f = 0; f < 3; f++)
            {
                for (int x = 0; x < board->width; x++)
                {
                    mvaddch(y, x, (f % 2) ? '*' : '#');
                }
                refresh();
                usleep(100000);
            }
            for (int yy = y; yy > 0; yy--)
            {
                for (int x = 0; x < board->width; x++)
                {
                    board->cells[yy * board->width + x] =
                        board->cells[(yy - 1) * board->width + x];
                }
            }
            for (int x = 0; x < board->width; x++)
                board->cells[x] = '0';
            cleared++;
        }
    }
    return cleared;
}

// ****************************************************************************
// Display the game over summary screen.
// Shows time played, lines cleared, difficulty multiplier, and final score,
// and asks the player whether to play again. */
// ****************************************************************************   
void showGameOverScreen(int score, int elapsed, int multiplier) {
    clear();

    int finalScore = score * elapsed * multiplier;

    mvprintw(0, 0, "GAME OVER");
    mvprintw(1, 0, "Time played: %d seconds", elapsed);
    mvprintw(2, 0, "Lines cleared: %d", score);
    mvprintw(3, 0, "Difficulty multiplier: x%d", multiplier);
    mvprintw(4, 0, "Final score: %d", finalScore);
    mvprintw(6, 0, "Play again? (Y/N)");

    refresh();
}

// ****************************************************************************
// Program entry point: initialize the board and ncurses, run the main game loop,
// handle input, gravity, piece spawning, scoring, and restarting. 
// ****************************************************************************
int main(int argc, char *argv[])
{
    int width = 16;                   // Default width
    int height = 32;                  // Default height
    int multiplier = 3;               // Default multiplier

    useconds_t tick = 50000;          // 50ms, the lenght of a game cycle
    useconds_t dropDelay = 200000;    // difficulty-based delay
    useconds_t elapsedDrop = 0;

    // Process the parameters passed in to override default values.
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "--width") == 0 || strcmp(argv[i], "-w") == 0) && i + 1 < argc)
        {
            width = atoi(argv[++i]);
        }
        else if ((strcmp(argv[i], "--height") == 0 || strcmp(argv[i], "-h") == 0) && i + 1 < argc)
        {
            height = atoi(argv[++i]);
        }
        else if ((strcmp(argv[i], "--difficulty") == 0 || strcmp(argv[i], "-d") == 0) && i + 1 < argc)
        {
            char *diff = argv[++i];
            if (strcmp(diff, "hard") == 0)
            {
                dropDelay = 200000;
                multiplier = 3;
            }
            else if (strcmp(diff, "medium") == 0)
            {
                dropDelay = 300000;
                multiplier = 2;
            }
            else if (strcmp(diff, "easy") == 0)
            {
                dropDelay = 700000;
                multiplier = 1;
            }
        }
    }

    // Create the gameboard
    GameBoard *board = createBoard(width, height);

    initscr();                   // Initialise the screen (ncurses)
    noecho();                    // Disable echo (ncurses)
    curs_set(FALSE);             // Set cursor visibility
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    srand((unsigned)time(NULL)); // Initialise the random function

    int playAgain = 1;           // Assume the user wants to play ...

    while (playAgain)            // ... and loop until they dont.
    {
        GameBoard *board = createBoard(width, height);
        nodelay(stdscr, TRUE);

        int gameOver = 0;               // Assume the game isn't over.
        int score = 0;                  // Set the starting score.
        time_t startTime = time(NULL);  // Remember the start time.

        while (!gameOver)               // Loop until the game is over ...
        {
            // Spawn a shape at the top of the screen in the middle.
            Shape current = tetrominoes[rand() % 7];
            int ox = width / 2 - 2;
            int oy = 0;

            // Check for a collision (with the top of the board) and end the
            // game if detected.
            if (collision(board, &current, ox, oy))
            {
                gameOver = 1;
                break;
            }

            // No collision so must be falling ok
            int falling = 1;

            // While a piece is falling
            while (falling)
            {
                // Keep track of the time and update the board
                int elapsed = (int)(time(NULL) - startTime);
                printBoard(board, score, elapsed);
                
                for (int i = 0; i < 4; i++)
                {
                    int bx = ox + current.x[i];
                    int by = oy + current.y[i];
                    if (by >= 0)
                        mvaddch(by, bx, '*');
                }

                refresh();

                // Input every tick
                int ch = getch();
                if (ch == KEY_LEFT && !collision(board, &current, ox - 1, oy))
                {
                    ox--;
                }
                else if (ch == KEY_RIGHT && !collision(board, &current, ox + 1, oy))
                {
                    ox++;
                }
                else if (ch == KEY_UP)
                {
                    Shape tmp = current;
                    rotateClockwise(&tmp);
                    if (!collision(board, &tmp, ox, oy))
                        current = tmp;
                }
                else if (ch == KEY_DOWN)
                {
                    while (!collision(board, &current, ox, oy + 1))
                        oy++;
                }

                // gravity only when enough ticks have passed
                elapsedDrop += tick;
                if (elapsedDrop >= dropDelay)
                {
                    if (!collision(board, &current, ox, oy + 1))
                    {
                        oy++;
                    }
                    else
                    {
                        lockShape(board, &current, ox, oy);
                        score += clearLines(board);
                        falling = 0;
                    }
                    elapsedDrop = 0; // reset counter
                }
                usleep(tick); // short sleep keeps input responsive
            }
        }

        // when game ends:
        clear();
        int elapsed = (int)(time(NULL) - startTime);
        showGameOverScreen(width, height, score, elapsed, multiplier);

        nodelay(stdscr, FALSE);
        int ch = getch();
        if (ch == 'Y' || ch == 'y')
        {
            playAgain = 1; // restart
        }
        else
        {
            playAgain = 0; // quit
        }

        freeBoard(board);
    }

    endwin();
    return 0;
}
