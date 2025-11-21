#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <time.h>

typedef struct {
    int width;
    int height;
    char *cells;
} GameBoard;

GameBoard *createBoard(int width, int height) {
    GameBoard *board = malloc(sizeof(GameBoard));
    board->width = width;
    board->height = height;
    board->cells = malloc(width * height);
    memset(board->cells, '0', width * height);
    return board;
}

void freeBoard(GameBoard *board) {
    free(board->cells);
    free(board);
}

void setCell(GameBoard *board, int x, int y, char value) {
    if (x >= 0 && x < board->width && y >= 0 && y < board->height) {
        board->cells[y * board->width + x] = value;
    }
}

char getCell(GameBoard *board, int x, int y) {
    if (x >= 0 && x < board->width && y >= 0 && y < board->height) {
        return board->cells[y * board->width + x];
    }
    return '?';
}

void printBoard(GameBoard *board, int score, int elapsed) {
    clear();
    for (int y = 0; y < board->height; y++) {
        for (int x = 0; x < board->width; x++) {
            char c = board->cells[y * board->width + x];
            mvaddch(y, x, (c == '0') ? '.' : '#');
        }
    }
    mvprintw(board->height + 1, 0, "Score: %d", score);
    mvprintw(board->height + 2, 0, "Time: %d seconds", elapsed);
    refresh();
}

typedef struct {
    int x[4];
    int y[4];
} Shape;

Shape tetrominoes[7] = {
    {{0,1,2,3},{0,0,0,0}}, // I
    {{0,1,0,1},{0,0,1,1}}, // O
    {{0,1,2,1},{0,0,0,1}}, // T
    {{1,2,0,1},{0,0,1,1}}, // S
    {{0,1,1,2},{0,0,1,1}}, // Z
    {{0,0,1,2},{0,1,1,1}}, // J
    {{2,0,1,2},{0,1,1,1}}  // L
};

void rotateClockwise(Shape *s) {
    for (int i = 0; i < 4; i++) {
        int oldX = s->x[i], oldY = s->y[i];
        s->x[i] = oldY;
        s->y[i] = -oldX;
    }
    int minX = s->x[0], minY = s->y[0];
    for (int i = 1; i < 4; i++) {
        if (s->x[i] < minX) minX = s->x[i];
        if (s->y[i] < minY) minY = s->y[i];
    }
    for (int i = 0; i < 4; i++) {
        s->x[i] -= minX;
        s->y[i] -= minY;
    }
}

int collision(GameBoard *board, Shape *s, int ox, int oy) {
    for (int i = 0; i < 4; i++) {
        int bx = ox + s->x[i];
        int by = oy + s->y[i];
        if (bx < 0 || bx >= board->width || by >= board->height) return 1;
        if (by >= 0 && getCell(board, bx, by) != '0') return 1;
    }
    return 0;
}

void lockShape(GameBoard *board, Shape *s, int ox, int oy) {
    for (int i = 0; i < 4; i++) {
        int bx = ox + s->x[i];
        int by = oy + s->y[i];
        if (bx >= 0 && bx < board->width && by >= 0 && by < board->height) {
            setCell(board, bx, by, '1');
        }
    }
}

int clearLines(GameBoard *board) {
    int cleared = 0;
    for (int y = 0; y < board->height; y++) {
        int full = 1;
        for (int x = 0; x < board->width; x++) {
            if (getCell(board, x, y) == '0') { full = 0; break; }
        }
        if (full) {
            for (int f = 0; f < 3; f++) {
                for (int x = 0; x < board->width; x++) {
                    mvaddch(y, x, (f % 2) ? '*' : '#');
                }
                refresh();
                usleep(100000);
            }
            for (int yy = y; yy > 0; yy--) {
                for (int x = 0; x < board->width; x++) {
                    board->cells[yy * board->width + x] =
                        board->cells[(yy - 1) * board->width + x];
                }
            }
            for (int x = 0; x < board->width; x++) board->cells[x] = '0';
            cleared++;
        }
    }
    return cleared;
}

void showGameOverScreen(int width, int height, int score, int elapsed, int multiplier) {
    int boxW = 30;
    int boxH = 8;
    int startX = (width - boxW) / 2;
    int startY = (height - boxH) / 2;

    // clear box area
    for (int y = 0; y < boxH; y++) {
        for (int x = 0; x < boxW; x++) {
            mvaddch(startY + y, startX + x, ' ');
        }
    }

    int finalScore = score * elapsed * multiplier;

    mvprintw(startY + 1, startX + 2, "GAME OVER");
    mvprintw(startY + 2, startX + 2, "Time played: %d seconds", elapsed);
    mvprintw(startY + 3, startX + 2, "Lines cleared: %d", score);
    mvprintw(startY + 4, startX + 2, "Difficulty multiplier: x%d", multiplier);
    mvprintw(startY + 5, startX + 2, "Final score: %d", finalScore);
    mvprintw(startY + 7, startX + 2, "Play again? (Y/N)");

    refresh();
}

int main(int argc, char *argv[]) {
    int width = 16;
    int height = 32;
    int multiplier = 3; // default hard
    //useconds_t dropDelay = 200000; // hard default (current speed)

    useconds_t tick = 50000; // 50ms tick
    useconds_t dropDelay = 200000; // difficulty-based delay
    useconds_t elapsedDrop = 0;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--width") == 0 || strcmp(argv[i], "-w") == 0) && i + 1 < argc) {
            width = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--height") == 0 || strcmp(argv[i], "-h") == 0) && i + 1 < argc) {
            height = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--difficulty") == 0 || strcmp(argv[i], "-d") == 0) && i + 1 < argc) {
            char *diff = argv[++i];
            if (strcmp(diff, "hard") == 0) {
	        dropDelay = 200000; 
		multiplier = 3 ;
	    } else if (strcmp(diff, "medium") == 0) {
   	        dropDelay = 300000; 
		multiplier = 2 ;
	    } else if (strcmp(diff, "easy") == 0) {
		dropDelay = 700000; 
		multiplier = 1 ;
	    }
        }
    }

    GameBoard *board = createBoard(width, height);

    initscr();
    noecho();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    srand((unsigned)time(NULL));

    int playAgain = 1;

    while (playAgain) {
        GameBoard *board = createBoard(width, height);
        nodelay(stdscr, TRUE);

        int gameOver = 0;
	    int score = 0;
	    time_t startTime = time(NULL);

	    while (!gameOver) {
            Shape current = tetrominoes[rand() % 7];
            int ox = width / 2 - 2;
            int oy = 0;

            if (collision(board, &current, ox, oy)) {
                gameOver = 1;
                break;
            }

		    int falling = 1;

            while (falling) {
                int elapsed = (int)(time(NULL) - startTime);
                printBoard(board, score, elapsed);
                for (int i = 0; i < 4; i++) {
                    int bx = ox + current.x[i];
                    int by = oy + current.y[i];
                    if (by >= 0) mvaddch(by, bx, '*');
                }
                refresh();

                // input every tick
                int ch = getch();
                if (ch == KEY_LEFT && !collision(board, &current, ox - 1, oy)) {
                    ox--;
                } else if (ch == KEY_RIGHT && !collision(board, &current, ox + 1, oy)) {
                    ox++;
                } else if (ch == KEY_UP) {
                    Shape tmp = current;
                    rotateClockwise(&tmp);
                    if (!collision(board, &tmp, ox, oy)) current = tmp;
                } else if (ch == KEY_DOWN) {
                    while (!collision(board, &current, ox, oy + 1)) oy++;
                }

                // gravity only when enough ticks have passed
                elapsedDrop += tick;
                if (elapsedDrop >= dropDelay) {
                    if (!collision(board, &current, ox, oy + 1)) {
                        oy++;
                    } else {
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
        if (ch == 'Y' || ch == 'y') {
            playAgain = 1; // restart
        } else {
            playAgain = 0; // quit
        }

        freeBoard(board);
    }

    endwin();
    return 0;
}


