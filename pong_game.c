#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <time.h>
#endif

#define BOARD_WIDTH 70
#define BOARD_HEIGHT 22
#define WIN_SCORE 7
#define FRAME_DELAY_MS 35
#define PADDLE_HEIGHT 4
#define PLAYER_X 2
#define AI_X (BOARD_WIDTH - 3)
#define SAVE_FILE "pong_save.txt"

typedef struct {
    int x;
    int y;
    int vx;
    int vy;
} Ball;

typedef struct {
    int y;
    int score;
} Paddle;

typedef struct {
    Ball ball;
    Paddle player;
    Paddle ai;
    int difficulty;      /* 1 = easy, 2 = medium, 3 = hard */
    int roundNumber;
    int paused;
    int playerWonLast;
    int running;
} GameState;

#ifndef _WIN32
static struct termios oldTermios;
#endif

void clearScreen(void);
void sleepMs(int ms);
void setupTerminal(void);
void restoreTerminal(void);
int keyPressed(void);
int readKey(void);
void clampPaddle(Paddle *paddle);
void resetBall(GameState *game, int direction);
void initialiseGame(GameState *game, int difficulty);
void drawGame(const GameState *game);
void updateBall(GameState *game);
void updateAi(GameState *game);
void movePlayer(Paddle *player, int direction);
void showTitleScreen(void);
int showMainMenu(void);
int chooseDifficulty(void);
void processInput(GameState *game, int *requestQuitToMenu);
void playGame(GameState *game);
void showInstructions(void);
void showGameOver(const GameState *game);
int saveGame(const GameState *game);
int loadGame(GameState *game);
void waitForEnter(void);
void drawCenteredText(const char *text, int width);

void clearScreen(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
}

void sleepMs(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

void setupTerminal(void) {
#ifndef _WIN32
    struct termios newTermios;
    tcgetattr(STDIN_FILENO, &oldTermios);
    newTermios = oldTermios;
    newTermios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
#endif
}

void restoreTerminal(void) {
#ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
#endif
}

int keyPressed(void) {
#ifdef _WIN32
    return _kbhit();
#else
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
#endif
}

int readKey(void) {
#ifdef _WIN32
    return _getch();
#else
    unsigned char ch;
    if (read(STDIN_FILENO, &ch, 1) <= 0) {
        return -1;
    }
    return ch;
#endif
}

void clampPaddle(Paddle *paddle) {
    if (paddle->y < 1) {
        paddle->y = 1;
    }
    if (paddle->y > BOARD_HEIGHT - PADDLE_HEIGHT - 1) {
        paddle->y = BOARD_HEIGHT - PADDLE_HEIGHT - 1;
    }
}

void resetBall(GameState *game, int direction) {
    int verticalOptions[3] = {-1, 0, 1};
    game->ball.x = BOARD_WIDTH / 2;
    game->ball.y = BOARD_HEIGHT / 2;
    game->ball.vx = direction;
    game->ball.vy = verticalOptions[rand() % 3];
    if (game->ball.vy == 0) {
        game->ball.vy = (rand() % 2 == 0) ? -1 : 1;
    }
}

void initialiseGame(GameState *game, int difficulty) {
    memset(game, 0, sizeof(*game));
    game->difficulty = difficulty;
    game->player.y = (BOARD_HEIGHT - PADDLE_HEIGHT) / 2;
    game->ai.y = (BOARD_HEIGHT - PADDLE_HEIGHT) / 2;
    game->roundNumber = 1;
    game->paused = 0;
    game->running = 1;
    resetBall(game, (rand() % 2 == 0) ? 1 : -1);
}

void drawCenteredText(const char *text, int width) {
    int len = (int)strlen(text);
    int padding = (width - len) / 2;
    int i;
    for (i = 0; i < padding; i++) {
        putchar(' ');
    }
    printf("%s\n", text);
}

void showTitleScreen(void) {
    clearScreen();
    printf("==============================================================\n");
    drawCenteredText("PONG ARENA", 62);
    drawCenteredText("B37VB Individual Software Game Project", 62);
    printf("==============================================================\n\n");
    printf("Features:\n");
    printf("- Real-time paddle movement and ball physics\n");
    printf("- Three AI difficulty levels\n");
    printf("- Scoreboard and first-to-7 win condition\n");
    printf("- Pause, restart, save and load\n");
    printf("- Console graphics built in C\n\n");
    printf("Player controls: W = up, S = down\n");
    printf("Game controls  : P = pause, R = restart round, K = save, Q = menu\n\n");
    printf("Press Enter to continue...");
    waitForEnter();
}

void showInstructions(void) {
    clearScreen();
    printf("=========================== HOW TO PLAY ===========================\n\n");
    printf("Objective:\n");
    printf("  Score by sending the ball past the AI paddle on the right.\n");
    printf("  The first side to reach %d points wins the match.\n\n", WIN_SCORE);
    printf("Controls:\n");
    printf("  W  - move paddle up\n");
    printf("  S  - move paddle down\n");
    printf("  P  - pause/resume\n");
    printf("  R  - restart the current match\n");
    printf("  K  - save the current game to file\n");
    printf("  Q  - return to the main menu\n\n");
    printf("Difficulty settings:\n");
    printf("  Easy   - slower AI with more mistakes\n");
    printf("  Medium - balanced opponent\n");
    printf("  Hard   - faster and more accurate AI\n\n");
    printf("Gameplay notes:\n");
    printf("  Hitting different parts of the paddle changes the ball angle.\n");
    printf("  After every point the ball resets in the centre.\n");
    printf("  Saving stores score, paddle positions, ball position and difficulty.\n\n");
    printf("Press Enter to return to the menu...");
    waitForEnter();
}

int chooseDifficulty(void) {
    char choice[16];
    while (1) {
        clearScreen();
        printf("========================= SELECT DIFFICULTY =========================\n\n");
        printf("1. Easy\n");
        printf("2. Medium\n");
        printf("3. Hard\n\n");
        printf("Enter option: ");
        if (!fgets(choice, sizeof(choice), stdin)) {
            continue;
        }
        if (choice[0] >= '1' && choice[0] <= '3') {
            return choice[0] - '0';
        }
    }
}

int showMainMenu(void) {
    char choice[16];
    while (1) {
        clearScreen();
        printf("============================= MAIN MENU =============================\n\n");
        printf("1. New Game\n");
        printf("2. Load Saved Game\n");
        printf("3. Instructions\n");
        printf("4. Exit\n\n");
        printf("Enter option: ");
        if (!fgets(choice, sizeof(choice), stdin)) {
            continue;
        }
        switch (choice[0]) {
            case '1': return 1;
            case '2': return 2;
            case '3': return 3;
            case '4': return 4;
            default: break;
        }
    }
}

void drawGame(const GameState *game) {
    int y, x;
    clearScreen();
    printf("PONG ARENA  |  Player: %d  AI: %d  |  First to %d  |  Difficulty: ",
           game->player.score, game->ai.score, WIN_SCORE);
    if (game->difficulty == 1) {
        printf("Easy\n");
    } else if (game->difficulty == 2) {
        printf("Medium\n");
    } else {
        printf("Hard\n");
    }
    printf("Controls: W/S move, P pause, R restart, K save, Q menu\n");
    if (game->paused) {
        printf("STATUS: PAUSED\n");
    } else {
        printf("STATUS: LIVE MATCH (Round %d)\n", game->roundNumber);
    }

    for (y = 0; y < BOARD_HEIGHT; y++) {
        for (x = 0; x < BOARD_WIDTH; x++) {
            char cell = ' ';

            if (y == 0 || y == BOARD_HEIGHT - 1) {
                cell = '#';
            } else if (x == BOARD_WIDTH / 2) {
                cell = ':';
            }

            if (x == PLAYER_X && y >= game->player.y && y < game->player.y + PADDLE_HEIGHT) {
                cell = '|';
            }
            if (x == AI_X && y >= game->ai.y && y < game->ai.y + PADDLE_HEIGHT) {
                cell = '|';
            }
            if (x == game->ball.x && y == game->ball.y) {
                cell = 'O';
            }

            putchar(cell);
        }
        putchar('\n');
    }
}

void movePlayer(Paddle *player, int direction) {
    player->y += direction;
    clampPaddle(player);
}

void updateAi(GameState *game) {
    int target = game->ball.y - (PADDLE_HEIGHT / 2);
    int reactionDelay;
    int randomWindow;

    if (game->difficulty == 1) {
        reactionDelay = 4;
        randomWindow = 3;
    } else if (game->difficulty == 2) {
        reactionDelay = 2;
        randomWindow = 2;
    } else {
        reactionDelay = 1;
        randomWindow = 1;
    }

    if ((rand() % reactionDelay) != 0) {
        return;
    }

    target += (rand() % (randomWindow * 2 + 1)) - randomWindow;

    if (game->ai.y + PADDLE_HEIGHT / 2 < target) {
        game->ai.y++;
    } else if (game->ai.y + PADDLE_HEIGHT / 2 > target) {
        game->ai.y--;
    }

    clampPaddle(&game->ai);
}

void updateBall(GameState *game) {
    int nextX = game->ball.x + game->ball.vx;
    int nextY = game->ball.y + game->ball.vy;

    if (nextY <= 1 || nextY >= BOARD_HEIGHT - 2) {
        game->ball.vy *= -1;
        nextY = game->ball.y + game->ball.vy;
    }

    if (nextX == PLAYER_X && nextY >= game->player.y && nextY < game->player.y + PADDLE_HEIGHT) {
        int impact = nextY - game->player.y;
        game->ball.vx = 1;
        game->ball.vy = impact - 1;
        if (game->ball.vy == 0) {
            game->ball.vy = (rand() % 2 == 0) ? -1 : 1;
        }
        nextX = game->ball.x + game->ball.vx;
    }

    if (nextX == AI_X && nextY >= game->ai.y && nextY < game->ai.y + PADDLE_HEIGHT) {
        int impact = nextY - game->ai.y;
        game->ball.vx = -1;
        game->ball.vy = impact - 1;
        if (game->ball.vy == 0) {
            game->ball.vy = (rand() % 2 == 0) ? -1 : 1;
        }
        nextX = game->ball.x + game->ball.vx;
    }

    game->ball.x = nextX;
    game->ball.y = nextY;

    if (game->ball.x <= 0) {
        game->ai.score++;
        game->playerWonLast = 0;
        game->roundNumber++;
        resetBall(game, 1);
        sleepMs(500);
    } else if (game->ball.x >= BOARD_WIDTH - 1) {
        game->player.score++;
        game->playerWonLast = 1;
        game->roundNumber++;
        resetBall(game, -1);
        sleepMs(500);
    }
}

int saveGame(const GameState *game) {
    FILE *file = fopen(SAVE_FILE, "w");
    if (file == NULL) {
        return 0;
    }

    fprintf(file, "%d %d %d %d %d %d %d %d %d %d %d\n",
            game->ball.x,
            game->ball.y,
            game->ball.vx,
            game->ball.vy,
            game->player.y,
            game->player.score,
            game->ai.y,
            game->ai.score,
            game->difficulty,
            game->roundNumber,
            game->playerWonLast);

    fclose(file);
    return 1;
}

int loadGame(GameState *game) {
    FILE *file = fopen(SAVE_FILE, "r");
    if (file == NULL) {
        return 0;
    }

    if (fscanf(file, "%d %d %d %d %d %d %d %d %d %d %d",
               &game->ball.x,
               &game->ball.y,
               &game->ball.vx,
               &game->ball.vy,
               &game->player.y,
               &game->player.score,
               &game->ai.y,
               &game->ai.score,
               &game->difficulty,
               &game->roundNumber,
               &game->playerWonLast) != 11) {
        fclose(file);
        return 0;
    }

    fclose(file);
    game->paused = 0;
    game->running = 1;
    clampPaddle(&game->player);
    clampPaddle(&game->ai);
    return 1;
}

void processInput(GameState *game, int *requestQuitToMenu) {
    while (keyPressed()) {
        int key = readKey();
        if (key == -1) {
            return;
        }

#ifdef _WIN32
        if (key == 0 || key == 224) {
            int special = readKey();
            if (special == 72) {
                movePlayer(&game->player, -1);
            } else if (special == 80) {
                movePlayer(&game->player, 1);
            }
            continue;
        }
#else
        if (key == 27 && keyPressed()) {
            int second = readKey();
            if (second == '[' && keyPressed()) {
                int third = readKey();
                if (third == 'A') {
                    movePlayer(&game->player, -1);
                } else if (third == 'B') {
                    movePlayer(&game->player, 1);
                }
            }
            continue;
        }
#endif

        key = tolower(key);
        if (key == 'w') {
            movePlayer(&game->player, -1);
        } else if (key == 's') {
            movePlayer(&game->player, 1);
        } else if (key == 'p') {
            game->paused = !game->paused;
        } else if (key == 'r') {
            int difficulty = game->difficulty;
            initialiseGame(game, difficulty);
        } else if (key == 'k') {
            drawGame(game);
            if (saveGame(game)) {
                printf("\nGame saved to %s\n", SAVE_FILE);
            } else {
                printf("\nUnable to save game.\n");
            }
            sleepMs(700);
        } else if (key == 'q') {
            *requestQuitToMenu = 1;
        }
    }
}

void showGameOver(const GameState *game) {
    clearScreen();
    printf("============================= GAME OVER =============================\n\n");
    printf("Final score: Player %d - %d AI\n\n", game->player.score, game->ai.score);
    if (game->player.score > game->ai.score) {
        printf("Result: You won the match. Great job!\n\n");
    } else {
        printf("Result: The AI won the match. Try again!\n\n");
    }
    printf("Press Enter to return to the main menu...");
    waitForEnter();
}

void playGame(GameState *game) {
    int quitToMenu = 0;
    setupTerminal();

    while (!quitToMenu && game->player.score < WIN_SCORE && game->ai.score < WIN_SCORE) {
        drawGame(game);
        processInput(game, &quitToMenu);

        if (!game->paused && !quitToMenu) {
            updateAi(game);
            updateBall(game);
        }
        sleepMs(FRAME_DELAY_MS);
    }

    restoreTerminal();

    if (!quitToMenu) {
        showGameOver(game);
    }
}

void waitForEnter(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
        /* discard */
    }
}

int main(void) {
    int menuChoice;
    GameState game;

    srand((unsigned int)time(NULL));

    showTitleScreen();

    while (1) {
        menuChoice = showMainMenu();

        if (menuChoice == 1) {
            int difficulty = chooseDifficulty();
            initialiseGame(&game, difficulty);
            playGame(&game);
        } else if (menuChoice == 2) {
            if (loadGame(&game)) {
                playGame(&game);
            } else {
                clearScreen();
                printf("No saved game was found.\n\nPress Enter to return to the menu...");
                waitForEnter();
            }
        } else if (menuChoice == 3) {
            showInstructions();
        } else if (menuChoice == 4) {
            clearScreen();
            printf("Thanks for playing Pong Arena!\n");
            break;
        }
    }

    return 0;
}
