// ****************************************************************************
// TETRIZ - A Tetris Clone
// =======================
// Just a text based Tetris Clone.
//
// Modification History
// ====================
// When       Who                  Why
// ========== ==================== ============================================
// 26/11/2025 Dave Hol'            Initial check in
// 29/11/2025 Dave Hol'            Added the first 8 TODO items
// 
// Notes
// =====
// Compile using:
//    gcc tetriz.c -o ~/bin/tetriz -lncurses
//
// TODO List
// =========
// 1. Check the screen size can accommodate the parameters passed               - Done
// 2. Enforce a minimum screen size (16 wide by 20 tall)                        - Done
// 3. Add a --help parameter that explains the command line arguments           - Done
// 4. Make the pieces fall progressively quicker, depending on difficulty level - Done
// 5. Make the down key accelerate gravity rather than instantly move the piece - Done
// 6. Add "P" to pause                                                          - Done
// 7. Set default parameters                                                    - Done
// 8. Add a high score table (in /tmp maybe?)                                   - Done
// ****************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <time.h>
#include <sys/types.h>

#define TETRIZ_VERSION "1.0.1"
#define STATS_FILE "/tmp/tetriz_stats.txt"
#define MAX_NAME_LENGTH 20

// ****************************************************************************
// Display help information
// ****************************************************************************
void showHelp(const char *programName)
{
    printf("TETRIZ - A Text-Based Tetris Clone v%s\n\n", TETRIZ_VERSION);
    printf("USAGE:\n");
    printf("  %s [OPTIONS]\n\n", programName);
    printf("DESCRIPTION:\n");
    printf("  A simple terminal-based Tetris clone with configurable board dimensions\n");
    printf("  and difficulty levels.\n\n");
    printf("OPTIONS:\n");
    printf("  -w, --width <value>        Board width (minimum 16, default 16)\n");
    printf("  -h, --height <value>       Board height (minimum 20, default 20)\n");
    printf("  -d, --difficulty <level>   Difficulty level: easy, medium, hard (default medium)\n");
    printf("  -?, --help                 Display this help message\n");
    printf("  -v, --version              Display version information\n\n");
    printf("EXAMPLES:\n");
    printf("  %s                         # Play with default settings\n", programName);
    printf("  %s -w 20 -h 24             # Play with custom board size\n", programName);
    printf("  %s -d easy                 # Play on easy difficulty\n", programName);
    printf("  %s -w 18 -h 22 -d medium   # Custom size and difficulty\n\n", programName);
    printf("CONTROLS:\n");
    printf("  LEFT/RIGHT arrows          Move piece left/right\n");
    printf("  UP arrow                   Rotate piece clockwise\n");
    printf("  DOWN arrow                 Accelerate piece downward\n");
    printf("  P                          Pause/Resume game\n");
}

// ****************************************************************************
// Display version information
// ****************************************************************************
void showVersion()
{
    printf("TETRIZ version %s\n", TETRIZ_VERSION);
}

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
// Display the gameboard (simple non-fancy version)
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

// ****************************************************************************
// Display the next piece preview to the right of the board
// ****************************************************************************
void printNextPiece(Shape *next, int boardWidth)
{
    const int previewX = boardWidth + 3;
    const int previewY = 1;

    mvprintw(previewY - 1, previewX, "NEXT:");
    
    // Draw the next piece
    for (int i = 0; i < 4; i++)
    {
        int nx = previewX + next->x[i];
        int ny = previewY + next->y[i];
        mvaddch(ny, nx, '#');
    }
    
    refresh();
}

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
// Prompt the player to enter their name and save game stats to file
// ****************************************************************************
void saveGameStats(int score, int elapsed, int multiplier, int linesCleared)
{
    char playerName[MAX_NAME_LENGTH + 1] = "John Doe";
    
    // Prompt for player name
    clear();
    mvprintw(0, 0, "Enter your name (max %d characters):", MAX_NAME_LENGTH);
    mvprintw(1, 0, "> ");
    refresh();
    
    nodelay(stdscr, FALSE);
    echo();
    
    char input[MAX_NAME_LENGTH + 1];
    if (getnstr(input, MAX_NAME_LENGTH) != ERR && strlen(input) > 0)
    {
        strncpy(playerName, input, MAX_NAME_LENGTH);
        playerName[MAX_NAME_LENGTH] = '\0';
    }
    
    noecho();
    nodelay(stdscr, TRUE);
    
    // Get current timestamp
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    // Calculate final score
    int finalScore = score * elapsed * multiplier;
    
    // Determine if file exists
    FILE *file = fopen(STATS_FILE, "a");
    if (file == NULL)
    {
        fprintf(stderr, "Error: Could not open stats file %s\n", STATS_FILE);
        return;
    }
    
    // Check if file is empty to write header
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    
    if (fileSize == 0)
    {
        // Write header
        fprintf(file, "%-19s | %-20s | %7s | %5s | %7s | %10s\n", 
                "Timestamp", "Player Name", "Elapsed", "Lines", "Diff", "Score");
        fprintf(file, "%-19s-+-%-20s-+-%-7s-+-%-5s-+-%-7s-+-%-10s\n",
                "-------------------", "--------------------", "-------", "-----", "-------", "----------");
    }
    
    // Write game stats
    char diffName[10];
    if (multiplier == 1)
        strcpy(diffName, "Easy");
    else if (multiplier == 2)
        strcpy(diffName, "Medium");
    else
        strcpy(diffName, "Hard");
    
    fprintf(file, "%-19s | %-20s | %7d | %5d | %7s | %10d\n",
            timestamp, playerName, elapsed, score, diffName, finalScore);
    
    fclose(file);
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
    int height = 20;                  // Default height
    int multiplier = 2;               // Default multiplier - Normal

    useconds_t tick = 50000;          // 50ms, the lenght of a game cycle
    useconds_t dropDelay = 200000;    // difficulty-based delay
    useconds_t elapsedDrop = 0;

    // Process the parameters passed in to override default values.
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-?") == 0))
        {
            showHelp(argv[0]);
            return 0;
        }
        else if ((strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0))
        {
            showVersion();
            return 0;
        }
        else if ((strcmp(argv[i], "--width") == 0 || strcmp(argv[i], "-w") == 0) && i + 1 < argc)
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
                dropDelay = 400000;
                multiplier = 2;
            }
            else if (strcmp(diff, "easy") == 0)
            {
                dropDelay = 700000;
                multiplier = 1;
            }
        }
    }

    // Validate and constrain dimensions
    const int MIN_WIDTH = 16;
    const int MIN_HEIGHT = 20;

    if (width < MIN_WIDTH)
    {
        fprintf(stderr, "Warning: width %d is less than minimum %d. Using minimum.\n", width, MIN_WIDTH);
        width = MIN_WIDTH;
    }

    if (height < MIN_HEIGHT)
    {
        fprintf(stderr, "Warning: height %d is less than minimum %d. Using minimum.\n", height, MIN_HEIGHT);
        height = MIN_HEIGHT;
    }

    // Initialize ncurses to check screen size
    initscr();
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);

    // Account for score/time display below the board (2 extra lines)
    int requiredHeight = height + 2;

    if (width > maxX || requiredHeight > maxY)
    {
        endwin();
        fprintf(stderr, "Error: Board dimensions (%d x %d) do not fit in terminal (%d x %d).\n",
                width, requiredHeight, maxX, maxY);
        fprintf(stderr, "Please resize your terminal or reduce the board dimensions.\n");
        return 1;
    }

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
        useconds_t currentDropDelay = dropDelay;  // Track current difficulty
        int nextShapeIndex = rand() % 7;  // Initialize the first next piece

        while (!gameOver)               // Loop until the game is over ...
        {
            // Spawn the next shape and pick a new 'next'
            Shape current = tetrominoes[nextShapeIndex];
            nextShapeIndex = rand() % 7;
            Shape nextShape = tetrominoes[nextShapeIndex];
            
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

            // Track whether the down key was active in the previous tick so we can
            // cancel any leftover accelerated timing when it's released.
            int downActive = 0;

            // Declare elapsed here so it's available throughout the piece's lifetime
            int elapsed = 0;

            // Display the next piece once when it's spawned
            printBoard(board, score, elapsed);
            printNextPiece(&nextShape, width);

            // While a piece is falling
            while (falling)
            {
                // Keep track of the time and update the board
                elapsed = (int)(time(NULL) - startTime);
                
                // Increase drop speed every 10 seconds (minimum 50000 microseconds)
                useconds_t speedBonus = (elapsed / 30) * 20000;
                currentDropDelay = dropDelay - speedBonus;
                if (currentDropDelay < 50000)
                    currentDropDelay = 50000;
                
                printBoard(board, score, elapsed);
                
                // Redraw the next piece preview
                printNextPiece(&nextShape, width);
                
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
                int speedMultiplier = 1; // normal speed 

                // Handle pause
                if (ch == 'P' || ch == 'p')
                {
                    nodelay(stdscr, FALSE);
                    mvprintw(height / 2, width / 2 - 4, "PAUSED");
                    refresh();
                    getch(); // Wait for any key to resume
                    nodelay(stdscr, TRUE);
                    continue; // Skip the rest of this tick
                }

                // Determine whether down is currently pressed
                int keyDown = (ch == KEY_DOWN);

		// Left key pressed
                if (ch == KEY_LEFT && !collision(board, &current, ox - 1, oy))
                {
                    ox--;
                }

		// Right key pressed
                else if (ch == KEY_RIGHT && !collision(board, &current, ox + 1, oy))
                {
                    ox++;
                }

		// Up key pressed
                else if (ch == KEY_UP)
                {
                    Shape tmp = current;
                    rotateClockwise(&tmp);
                    if (!collision(board, &tmp, ox, oy))
                        current = tmp;
                }

		// Down key handling (initial press and hold handled below)
                // (removed redundant 'else if (ch == KEY_DOWN)' branch)

                else if (keyDown)
                {
                    // accelerate while the key is held
                    speedMultiplier = 10; // Increase the speed downwards
                    downActive = 1;
                }
                else
                {
                    // If we just released the down key, reset the accumulated drop
                    // time so acceleration stops immediately.
                    if (downActive)
                    {
                        downActive = 0;
                        elapsedDrop = 0;
                    }
                }
 
                // gravity using an effective delay so we don't over-accumulate
                // while accelerating. We always add the base tick and compare
                // against currentDropDelay/speedMultiplier.
                elapsedDrop += tick;
                useconds_t effectiveDelay = currentDropDelay / speedMultiplier;
                if (elapsedDrop >= effectiveDelay)
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

        // Save game stats to file (prompts for player name)
        saveGameStats(score, elapsed, multiplier, score);

        // Now show summary and prompt to play again
        showGameOverScreen(score, elapsed, multiplier);

        nodelay(stdscr, FALSE);
        int ch = 0;
        while (ch != 'Y' && ch != 'y' && ch != 'N' && ch != 'n')
        {
            ch = getch();
        }
        
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

