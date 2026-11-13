/* maze.h - everything shared by the maze programs */
#ifndef MAZE_H
#define MAZE_H

#include <stdint.h>

/* the game is C++, so tell it these are C functions */
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ROWS 32
#define MAX_COLS 32
#define MAX_CELLS (MAX_ROWS * MAX_COLS)

#define STAMINA_MAX 100
#define FRUIT_STAMINA 20

/* ---------- maze generator (maze_generator.c) ---------- */

typedef enum { EASY, MEDIUM, HARD } Difficulty;

typedef struct {
    int rows;
    int cols;
    int loop_percent;   /* how many extra walls to break to make loops */
    int fruit_count;
} Level;

Level get_level(Difficulty diff);

/* fills grid with 1 = wall and 0 = path. returns 0 if the size is wrong */
int generate_maze(int *grid, int rows, int cols, int loop_percent, unsigned int seed);

/* ---------- graph (maze_core.c) ---------- */

/* one node of the adjacency list */
typedef struct Edge {
    int to;
    struct Edge *next;
} Edge;

typedef struct {
    int rows;
    int cols;
    int vertex_count;
    int cell_to_vertex[MAX_ROWS][MAX_COLS];   /* -1 if the cell is a wall */
    int vertex_row[MAX_CELLS];
    int vertex_col[MAX_CELLS];
    Edge *adjacency[MAX_CELLS];
} Graph;

void graph_init(Graph *g, int rows, int cols);
void graph_free(Graph *g);
int build_graph(Graph *g, const int *grid, int rows, int cols);
int vertex_at(const Graph *g, int row, int col);
int bfs_shortest_path(const Graph *g, int start_row, int start_col,
                      int goal_row, int goal_col, int *path, int max_path);
int dfs_count(const Graph *g, int start_row, int start_col);

/* ---------- features for the model (maze_core.c) ---------- */

#define FEATURE_COUNT 7

typedef struct {
    int vertices;
    int edges;
    int dead_ends;
    int junctions;
    int loops;
    int shortest_path;
    double detour_ratio;
} Features;

int get_features(const Graph *g, int start_row, int start_col,
                 int goal_row, int goal_col, Features *f);
void features_to_array(const Features *f, double out[FEATURE_COUNT]);

/* ---------- bots and game rules (maze_core.c) ---------- */

#define BOT_RANDOM 0
#define BOT_GREEDY 1

uint32_t next_random(uint32_t *state);
int stamina_after_move(int stamina);
int stamina_after_fruit(int stamina);

/* returns 1 = reached goal, 0 = failed, -1 = wrong input */
int run_bot(const Graph *g, int start, int goal, const int *fruits,
            int fruit_count, int bot, uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif
