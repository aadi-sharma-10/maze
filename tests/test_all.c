/* test_all.c - tests for the generator, graph, features and bots.
   if any assert fails the program stops and prints which line */
#include "maze.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_features(void)
{
    Graph g;
    Features f;
    int i;

    graph_init(&g, 1, 3);

    /* straight corridor: 3 cells in a row */
    int corridor[] = {0, 0, 0};
    assert(build_graph(&g, corridor, 1, 3));
    assert(get_features(&g, 0, 0, 0, 2, &f));
    assert(f.vertices == 3 && f.edges == 2 && f.dead_ends == 2);
    assert(f.junctions == 0 && f.loops == 0 && f.shortest_path == 2);
    assert(fabs(f.detour_ratio - 1.0) < 1e-12);

    /* start == goal */
    assert(get_features(&g, 0, 0, 0, 0, &f));
    assert(f.shortest_path == 0 && f.detour_ratio == 1.0);

    /* 2x2 open square has one loop */
    int square[] = {0, 0, 0, 0};
    assert(build_graph(&g, square, 2, 2));
    assert(get_features(&g, 0, 0, 1, 1, &f));
    assert(f.vertices == 4 && f.edges == 4 && f.loops == 1);
    assert(f.dead_ends == 0 && fabs(f.detour_ratio - sqrt(2.0)) < 1e-12);

    /* plus shape: one junction in the middle */
    int cross[] = {1, 0, 1,
                   0, 0, 0,
                   1, 0, 1};
    assert(build_graph(&g, cross, 3, 3));
    assert(get_features(&g, 0, 1, 2, 1, &f));
    assert(f.vertices == 5 && f.edges == 4 && f.dead_ends == 4 && f.junctions == 1);

    double arr[FEATURE_COUNT];
    double expected[] = {5, 4, 4, 1, 0, 2, 1};
    features_to_array(&f, arr);
    for (i = 0; i < FEATURE_COUNT; i++)
        assert(arr[i] == expected[i]);

    /* start is a wall */
    assert(!get_features(&g, 0, 0, 2, 1, &f));

    /* two cells with a wall between them = not connected */
    int split[] = {0, 1, 0};
    assert(build_graph(&g, split, 1, 3));
    assert(!get_features(&g, 0, 0, 0, 2, &f));
    assert(!get_features(&g, 0, 0, 0, 0, &f));   /* still not connected */
    assert(!get_features(NULL, 0, 0, 0, 0, &f));

    graph_free(&g);
    printf("Feature tests PASS\n");
}

static void test_bots(void)
{
    Graph g;
    int i, r, c;

    /* stamina rules */
    assert(stamina_after_move(1) == 0);
    assert(stamina_after_fruit(95) == 100);
    assert(stamina_after_fruit(0) == 20);

    /* snake corridor 4 x 31: 62 moves from (0,0) to (2,0) */
    int grid[4 * 31];
    for (i = 0; i < 4 * 31; i++)
        grid[i] = 1;
    for (c = 0; c < 31; c++)
    {
        grid[c] = 0;
        grid[62 + c] = 0;
    }
    grid[61] = 0;

    graph_init(&g, 4, 31);
    assert(build_graph(&g, grid, 4, 31));
    int start = vertex_at(&g, 0, 0);
    int goal = vertex_at(&g, 2, 0);
    assert(run_bot(&g, start, goal, NULL, 0, BOT_GREEDY, 123) == 1);
    /* same seed gives same result */
    int a = run_bot(&g, start, goal, NULL, 0, BOT_RANDOM, 123);
    assert(a == run_bot(&g, start, goal, NULL, 0, BOT_RANDOM, 123));
    assert(run_bot(&g, start, start, NULL, 0, BOT_RANDOM, 123) == 1);
    assert(run_bot(&g, -1, goal, NULL, 0, BOT_RANDOM, 123) == -1);
    graph_free(&g);

    /* longer snake 7 x 31: 126 moves, more than 100 stamina */
    int big[7 * 31];
    for (i = 0; i < 7 * 31; i++)
        big[i] = 1;
    for (r = 0; r < 7; r += 2)
        for (c = 0; c < 31; c++)
            big[r * 31 + c] = 0;
    big[1 * 31 + 30] = 0;
    big[3 * 31] = 0;
    big[5 * 31 + 30] = 0;

    assert(build_graph(&g, big, 7, 31));
    start = vertex_at(&g, 0, 0);
    int move100 = vertex_at(&g, 6, 26);
    int move101 = vertex_at(&g, 6, 25);
    assert(run_bot(&g, start, move100, NULL, 0, BOT_GREEDY, 7) == 1);   /* last stamina */
    assert(run_bot(&g, start, move101, NULL, 0, BOT_GREEDY, 7) == 0);   /* one too far */
    assert(run_bot(&g, start, move101, &move100, 1, BOT_GREEDY, 7) == 1);   /* fruit on move 100 */

    goal = vertex_at(&g, 6, 0);
    assert(run_bot(&g, start, goal, NULL, 0, BOT_GREEDY, 7) == 0);
    int fruit[2];
    fruit[0] = vertex_at(&g, 4, 10);
    fruit[1] = vertex_at(&g, 6, 20);
    assert(run_bot(&g, start, goal, fruit, 2, BOT_GREEDY, 7) == 1);
    assert(run_bot(&g, start, goal, fruit, 1, BOT_GREEDY, 7) == 0);   /* one fruit not enough */
    int same_fruit[2];
    same_fruit[0] = fruit[0];
    same_fruit[1] = fruit[0];
    assert(run_bot(&g, start, goal, same_fruit, 2, BOT_GREEDY, 7) == -1);
    graph_free(&g);

    printf("Simulation tests PASS\n");
}

/* make 100 mazes of each difficulty and check every one can be solved */
static void test_generator(void)
{
    const char *names[3] = {"EASY", "MEDIUM", "HARD"};
    int grid[MAX_CELLS];
    int path[MAX_CELLS];
    int d, i;

    for (d = 0; d < 3; d++)
    {
        Level level = get_level((Difficulty)d);
        for (i = 0; i < 100; i++)
        {
            unsigned int seed = (unsigned int)(i + 1) * 7919u;
            Graph g;
            graph_init(&g, level.rows, level.cols);

            if (!generate_maze(grid, level.rows, level.cols, level.loop_percent, seed) ||
                !build_graph(&g, grid, level.rows, level.cols))
            {
                printf("%s seed=%u: could not build maze\n", names[d], seed);
                assert(0);
            }
            int len = bfs_shortest_path(&g, 1, 1, level.rows - 2, level.cols - 2, path, MAX_CELLS);
            if (len <= 0 || dfs_count(&g, 1, 1) <= 0)
            {
                printf("%s seed=%u: no path to the exit\n", names[d], seed);
                assert(0);
            }
            graph_free(&g);
        }
    }
    printf("All 300 generated mazes are solvable. PASS\n");
}

int main(void)
{
    test_features();
    test_bots();
    test_generator();
    printf("All C tests PASS\n");
    return 0;
}
