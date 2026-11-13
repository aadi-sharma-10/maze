/* maze_generator.c - makes a random maze using DFS (recursive backtracker) */
#include "maze.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* mix up an array (Fisher-Yates shuffle) */
static void shuffle(int *arr, int n, uint32_t *rng)
{
    int i;
    for (i = n - 1; i > 0; i--)
    {
        int j = next_random(rng) % (i + 1);
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

Level get_level(Difficulty diff)
{
    Level level;
    if (diff == EASY)
    {
        level.rows = 15;
        level.cols = 15;
        level.loop_percent = 15;
        level.fruit_count = 5;
    }
    else if (diff == HARD)
    {
        level.rows = 31;
        level.cols = 31;
        level.loop_percent = 0;
        level.fruit_count = 3;
    }
    else   /* MEDIUM */
    {
        level.rows = 21;
        level.cols = 21;
        level.loop_percent = 5;
        level.fruit_count = 4;
    }
    return level;
}

/*
 * The maze has "cells" at odd positions (1,1), (1,3), (3,1) ...
 * and walls between them. We start at the first cell and keep going to a
 * random unvisited neighbour, breaking the wall between them. When there is
 * no unvisited neighbour we go back (pop the stack).
 */
int generate_maze(int *grid, int rows, int cols, int loop_percent, unsigned int seed)
{
    int dr[4] = {-1, 1, 0, 0};
    int dc[4] = {0, 0, -1, 1};
    int visited[MAX_CELLS] = {0};
    int stack[MAX_CELLS];
    int top = 0;
    int i, r, c;

    /* size must be odd and at least 3 */
    if (rows < 3 || cols < 3)
        return 0;
    if (rows % 2 == 0 || cols % 2 == 0)
        return 0;
    if (rows > MAX_ROWS || cols > MAX_COLS)
        return 0;
    if (loop_percent < 0)
        loop_percent = 0;
    if (loop_percent > 100)
        loop_percent = 100;

    /* seed 0 means "use the time" */
    uint32_t rng = seed;
    if (seed == 0)
        rng = (uint32_t)time(NULL);

    /* start with all walls */
    for (i = 0; i < rows * cols; i++)
        grid[i] = 1;

    int cell_rows = (rows - 1) / 2;
    int cell_cols = (cols - 1) / 2;

    /* start from cell 0, which is grid position (1,1) */
    visited[0] = 1;
    grid[1 * cols + 1] = 0;
    stack[top] = 0;
    top++;

    while (top > 0)
    {
        int cur = stack[top - 1];
        int cr = cur / cell_cols;
        int cc = cur % cell_cols;

        int order[4] = {0, 1, 2, 3};
        shuffle(order, 4, &rng);

        int moved = 0;
        for (i = 0; i < 4; i++)
        {
            int d = order[i];
            int nr = cr + dr[d];
            int nc = cc + dc[d];
            if (nr < 0 || nr >= cell_rows || nc < 0 || nc >= cell_cols)
                continue;

            int next = nr * cell_cols + nc;
            if (visited[next] == 1)
                continue;

            /* break the wall between the two cells */
            int wall_r = cr * 2 + 1 + dr[d];
            int wall_c = cc * 2 + 1 + dc[d];
            grid[wall_r * cols + wall_c] = 0;

            /* open the new cell */
            grid[(nr * 2 + 1) * cols + (nc * 2 + 1)] = 0;

            visited[next] = 1;
            stack[top] = next;
            top++;
            moved = 1;
            break;
        }

        if (moved == 0)   /* dead end, go back */
            top--;
    }

    /* now break some extra walls so the maze has loops */
    if (loop_percent > 0)
    {
        int walls[MAX_CELLS];
        int wall_count = 0;

        for (r = 1; r < rows - 1; r++)
        {
            for (c = 1; c < cols - 1; c++)
            {
                if (grid[r * cols + c] != 1)
                    continue;

                /* wall with path above and below */
                if (r % 2 == 0 && c % 2 == 1)
                {
                    if (grid[(r - 1) * cols + c] == 0 && grid[(r + 1) * cols + c] == 0)
                    {
                        walls[wall_count] = r * cols + c;
                        wall_count++;
                    }
                }
                /* wall with path left and right */
                else if (r % 2 == 1 && c % 2 == 0)
                {
                    if (grid[r * cols + c - 1] == 0 && grid[r * cols + c + 1] == 0)
                    {
                        walls[wall_count] = r * cols + c;
                        wall_count++;
                    }
                }
            }
        }

        shuffle(walls, wall_count, &rng);

        int remove = wall_count * loop_percent / 100;
        for (i = 0; i < remove; i++)
            grid[walls[i]] = 0;
    }

    return 1;
}

/* ------------------------------------------------------------------
 * Terminal version. Only compiled with -DMAZE_CLI (see "make cli"),
 * so the game and the dataset program don't get a second main().
 * ------------------------------------------------------------------ */
#ifdef MAZE_CLI

static void print_maze(const int *grid, int rows, int cols)
{
    int r, c;
    printf("\n=== Maze (%d x %d) ===\n\n", rows, cols);
    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            if (r == 1 && c == 1)
                printf("S");
            else if (r == rows - 2 && c == cols - 2)
                printf("E");
            else if (grid[r * cols + c] == 1)
                printf("#");
            else
                printf(" ");
        }
        printf("\n");
    }
    printf("\n");
}

int main(void)
{
    int grid[MAX_CELLS];
    char line[64];
    int choice;

    while (1)
    {
        printf("\n");
        printf("+----------------------------------+\n");
        printf("|     Maze Generator CLI Tool      |\n");
        printf("+----------------------------------+\n");
        printf("|  1) Easy   (15 x 15)             |\n");
        printf("|  2) Medium (21 x 21)             |\n");
        printf("|  3) Hard   (31 x 31)             |\n");
        printf("|  0) Exit                         |\n");
        printf("+----------------------------------+\n");
        printf("Select difficulty [0-3]: ");

        if (fgets(line, sizeof(line), stdin) == NULL)
            break;
        if (sscanf(line, "%d", &choice) != 1 || choice < 0 || choice > 3)
        {
            printf("Invalid input. Please enter 0, 1, 2, or 3.\n");
            continue;
        }
        if (choice == 0)
            break;

        Level level;
        if (choice == 1)
            level = get_level(EASY);
        else if (choice == 3)
            level = get_level(HARD);
        else
            level = get_level(MEDIUM);

        unsigned int seed = (unsigned int)time(NULL);
        printf("\nGenerating %dx%d maze (seed=%u, loops=%d%%) ...\n",
               level.rows, level.cols, seed, level.loop_percent);

        if (generate_maze(grid, level.rows, level.cols, level.loop_percent, seed) == 0)
        {
            printf("Error: maze generation failed.\n");
            continue;
        }
        print_maze(grid, level.rows, level.cols);
    }

    printf("Goodbye!\n");
    return 0;
}

#endif
