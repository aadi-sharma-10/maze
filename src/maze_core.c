/* maze_core.c - maze graph (BFS, DFS), features and the two bots */
#include "maze.h"
#include <math.h>
#include <stdlib.h>

/* ================= random numbers ================= */

/* xorshift: a simple random number generator.
   we use our own instead of rand() so the same seed gives the
   same maze on every computer */
uint32_t next_random(uint32_t *state)
{
    uint32_t x = *state;
    if (x == 0)
        x = 1;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    x = x ^ (x << 5);
    *state = x;
    return x;
}

/* ================= game rules ================= */

int stamina_after_move(int stamina)
{
    if (stamina > 0)
        return stamina - 1;
    return 0;
}

int stamina_after_fruit(int stamina)
{
    stamina = stamina + FRUIT_STAMINA;
    if (stamina > STAMINA_MAX)
        stamina = STAMINA_MAX;
    return stamina;
}

/* ================= graph ================= */

void graph_init(Graph *g, int rows, int cols)
{
    int i, j;
    g->rows = rows;
    g->cols = cols;
    g->vertex_count = 0;

    for (i = 0; i < MAX_ROWS; i++)
        for (j = 0; j < MAX_COLS; j++)
            g->cell_to_vertex[i][j] = -1;

    for (i = 0; i < MAX_CELLS; i++)
    {
        g->vertex_row[i] = -1;
        g->vertex_col[i] = -1;
        g->adjacency[i] = NULL;
    }
}

void graph_free(Graph *g)
{
    int i;
    for (i = 0; i < g->vertex_count; i++)
    {
        Edge *e = g->adjacency[i];
        while (e != NULL)
        {
            Edge *next = e->next;
            free(e);
            e = next;
        }
        g->adjacency[i] = NULL;
    }
    g->vertex_count = 0;
}

/* add edge from -> to at the front of the list */
static int add_edge(Graph *g, int from, int to)
{
    Edge *e = (Edge *)malloc(sizeof(Edge));
    if (e == NULL)
        return 0;
    e->to = to;
    e->next = g->adjacency[from];
    g->adjacency[from] = e;
    return 1;
}

/* every open cell (0) becomes a vertex,
   and two open cells next to each other get an edge */
int build_graph(Graph *g, const int *grid, int rows, int cols)
{
    int dr[4] = {-1, 1, 0, 0};   /* up, down, left, right */
    int dc[4] = {0, 0, -1, 1};
    int r, c, d;

    if (rows <= 0 || cols <= 0 || rows > MAX_ROWS || cols > MAX_COLS)
        return 0;

    graph_free(g);
    graph_init(g, rows, cols);

    /* step 1: give a number to every open cell */
    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            if (grid[r * cols + c] == 0)
            {
                int v = g->vertex_count;
                g->cell_to_vertex[r][c] = v;
                g->vertex_row[v] = r;
                g->vertex_col[v] = c;
                g->vertex_count++;
            }
        }
    }

    /* step 2: connect neighbours */
    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            int from = g->cell_to_vertex[r][c];
            if (from == -1)
                continue;

            for (d = 0; d < 4; d++)
            {
                int nr = r + dr[d];
                int nc = c + dc[d];
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols)
                    continue;

                int to = g->cell_to_vertex[nr][nc];
                if (to != -1)
                {
                    if (add_edge(g, from, to) == 0)
                    {
                        graph_free(g);
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

/* vertex number of a cell, or -1 */
int vertex_at(const Graph *g, int row, int col)
{
    if (row < 0 || row >= g->rows || col < 0 || col >= g->cols)
        return -1;
    return g->cell_to_vertex[row][col];
}

/* BFS using a queue. puts the path (start ... goal) in path[]
   and returns how many cells are in it, or 0 if there is no path */
int bfs_shortest_path(const Graph *g, int start_row, int start_col,
                      int goal_row, int goal_col, int *path, int max_path)
{
    int start = vertex_at(g, start_row, start_col);
    int goal = vertex_at(g, goal_row, goal_col);
    int queue[MAX_CELLS];
    int parent[MAX_CELLS];
    int visited[MAX_CELLS] = {0};
    int front = 0, back = 0;
    int i;

    if (start == -1 || goal == -1 || max_path <= 0)
        return 0;

    for (i = 0; i < g->vertex_count; i++)
        parent[i] = -1;

    visited[start] = 1;
    queue[back] = start;
    back++;

    while (front < back)
    {
        int current = queue[front];
        front++;
        if (current == goal)
            break;

        Edge *e;
        for (e = g->adjacency[current]; e != NULL; e = e->next)
        {
            if (visited[e->to] == 0)
            {
                visited[e->to] = 1;
                parent[e->to] = current;
                queue[back] = e->to;
                back++;
            }
        }
    }

    if (visited[goal] == 0)
        return 0;

    /* walk back from goal to start using parent[] */
    int reversed[MAX_CELLS];
    int count = 0;
    int at = goal;
    while (at != -1 && count < MAX_CELLS)
    {
        reversed[count] = at;
        count++;
        at = parent[at];
    }

    if (count > max_path)
        count = max_path;

    /* flip it so it goes start -> goal */
    for (i = 0; i < count; i++)
        path[i] = reversed[count - 1 - i];

    return count;
}

/* DFS using a stack. returns how many cells can be reached from start */
int dfs_count(const Graph *g, int start_row, int start_col)
{
    int start = vertex_at(g, start_row, start_col);
    int stack[MAX_CELLS];
    int visited[MAX_CELLS] = {0};
    int top = 0;
    int count = 0;

    if (start == -1)
        return 0;

    stack[top] = start;
    top++;
    visited[start] = 1;

    while (top > 0)
    {
        top--;
        int current = stack[top];
        count++;

        Edge *e;
        for (e = g->adjacency[current]; e != NULL; e = e->next)
        {
            if (visited[e->to] == 0)
            {
                visited[e->to] = 1;
                stack[top] = e->to;
                top++;
            }
        }
    }
    return count;
}

/* ================= features ================= */

/* works out the 7 numbers the model uses.
   returns 0 if the maze is not fully connected or the cells are wrong */
int get_features(const Graph *g, int sr, int sc, int gr, int gc, Features *f)
{
    int path[MAX_CELLS];
    int v;

    if (f == NULL)
        return 0;

    f->vertices = 0;
    f->edges = 0;
    f->dead_ends = 0;
    f->junctions = 0;
    f->loops = 0;
    f->shortest_path = 0;
    f->detour_ratio = 0;

    if (g == NULL || g->vertex_count <= 0)
        return 0;
    if (dfs_count(g, sr, sc) != g->vertex_count)   /* some cells cut off */
        return 0;

    int count = bfs_shortest_path(g, sr, sc, gr, gc, path, MAX_CELLS);
    if (count == 0)
        return 0;

    /* count neighbours (degree) of every vertex */
    int degree_sum = 0;
    for (v = 0; v < g->vertex_count; v++)
    {
        int degree = 0;
        Edge *e;
        for (e = g->adjacency[v]; e != NULL; e = e->next)
            degree++;

        degree_sum = degree_sum + degree;
        if (degree == 1)
            f->dead_ends++;
        if (degree >= 3)
            f->junctions++;
    }

    f->vertices = g->vertex_count;
    f->edges = degree_sum / 2;               /* every edge was counted twice */
    f->loops = f->edges - f->vertices + 1;
    f->shortest_path = count - 1;            /* moves, not cells */

    double distance = hypot(gr - sr, gc - sc);   /* straight line distance */
    if (distance > 0)
        f->detour_ratio = f->shortest_path / distance;
    else
        f->detour_ratio = 1.0;

    return 1;
}

void features_to_array(const Features *f, double out[FEATURE_COUNT])
{
    out[0] = f->vertices;
    out[1] = f->edges;
    out[2] = f->dead_ends;
    out[3] = f->junctions;
    out[4] = f->loops;
    out[5] = f->shortest_path;
    out[6] = f->detour_ratio;
}

/* ================= bots ================= */

static int manhattan(const Graph *g, int a, int b)
{
    return abs(g->vertex_row[a] - g->vertex_row[b]) +
           abs(g->vertex_col[a] - g->vertex_col[b]);
}

/* runs one bot from start to goal.
   returns 1 if it reaches the goal, 0 if stamina runs out or it is stuck,
   -1 if the input is wrong */
int run_bot(const Graph *g, int start, int goal, const int *fruits,
            int fruit_count, int bot, uint32_t seed)
{
    int has_fruit[MAX_CELLS] = {0};
    int visits[MAX_CELLS] = {0};
    int i;

    /* check the input first */
    if (g == NULL)
        return -1;
    if (start < 0 || start >= g->vertex_count)
        return -1;
    if (goal < 0 || goal >= g->vertex_count)
        return -1;
    if (fruit_count < 0 || fruit_count > g->vertex_count)
        return -1;
    if (fruit_count > 0 && fruits == NULL)
        return -1;
    if (bot != BOT_RANDOM && bot != BOT_GREEDY)
        return -1;

    /* mark where the fruits are */
    for (i = 0; i < fruit_count; i++)
    {
        int v = fruits[i];
        if (v < 0 || v >= g->vertex_count)
            return -1;
        if (has_fruit[v] == 1)   /* same fruit given two times */
            return -1;
        has_fruit[v] = 1;
    }

    int current = start;
    int stamina = STAMINA_MAX;
    visits[current] = 1;

    /* keep moving until we reach the goal or stamina is over */
    while (current != goal && stamina > 0)
    {
        int choices[4];
        int count = 0;
        Edge *e;

        if (bot == BOT_RANDOM)
        {
            /* random bot: every neighbour is a choice */
            for (e = g->adjacency[current]; e != NULL; e = e->next)
            {
                choices[count] = e->to;
                count++;
            }
        }
        else
        {
            /* greedy bot step 1: find the least visited neighbour,
               and if there is a tie, the one closest to the goal */
            int best_visits = -1;
            int best_distance = -1;
            for (e = g->adjacency[current]; e != NULL; e = e->next)
            {
                int v = e->to;
                int distance = manhattan(g, v, goal);
                if (best_visits == -1 || visits[v] < best_visits ||
                    (visits[v] == best_visits && distance < best_distance))
                {
                    best_visits = visits[v];
                    best_distance = distance;
                }
            }

            /* greedy bot step 2: all neighbours that are equally best */
            for (e = g->adjacency[current]; e != NULL; e = e->next)
            {
                int v = e->to;
                if (visits[v] == best_visits && manhattan(g, v, goal) == best_distance)
                {
                    choices[count] = v;
                    count++;
                }
            }
        }

        if (count == 0)   /* no way to go */
            return 0;

        /* pick one of the choices randomly */
        int pick = next_random(&seed) % count;
        current = choices[pick];
        visits[current]++;

        /* every move costs stamina */
        stamina = stamina_after_move(stamina);

        /* eat the fruit if there is one here */
        if (has_fruit[current] == 1)
        {
            has_fruit[current] = 0;
            stamina = stamina_after_fruit(stamina);
        }
    }

    if (current == goal)
        return 1;
    return 0;
}
