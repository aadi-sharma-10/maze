/*
 * maze_dataset.c - makes the training data for the difficulty model.
 *
 *   ./maze_dataset [seed_count] [trials_per_bot] [first_seed] > data.csv
 *       for every seed and difficulty: make a maze, find its features,
 *       run both bots many times and print one CSV line.
 */
#include "maze.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* reads a whole positive number from text. returns 0 if it is not valid */
static int read_number(const char *text, unsigned long max, unsigned long *out)
{
    char *end;
    if (text[0] == '-')
        return 0;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0)                     /* number too big */
        return 0;
    if (end == text || *end != '\0')   /* not a number, or junk after it */
        return 0;
    if (value == 0 || value > max)
        return 0;
    *out = value;
    return 1;
}

int main(int argc, char **argv)
{
    unsigned long seed_count = 200;
    unsigned long trials = 64;
    unsigned long first_seed = 1;
    int ok = 1;

    if (argc > 4)
        ok = 0;
    if (ok && argc > 1)
        ok = read_number(argv[1], 100000, &seed_count);
    if (ok && argc > 2)
        ok = read_number(argv[2], 100000, &trials);
    if (ok && argc > 3)
        ok = read_number(argv[3], 4294967295UL, &first_seed);
    /* last seed must still fit in 32 bits */
    if (ok && (unsigned long long)first_seed + seed_count - 1 > 4294967295ULL)
        ok = 0;

    if (!ok)
    {
        fprintf(stderr, "Usage: %s [seed_count=200] [trials_per_bot=64] [first_seed=1]\n", argv[0]);
        return 1;
    }

    printf("seed,difficulty,fruit_count,trials_per_bot,vertices,edges,dead_ends,"
           "junctions,loops,shortest_path,detour_ratio,random_successes,"
           "greedy_successes,success_rate\n");

    unsigned long s;
    int diff, bot, i;
    for (s = 0; s < seed_count; s++)
    {
        unsigned long seed = first_seed + s;

        for (diff = 0; diff < 3; diff++)
        {
            Level level = get_level((Difficulty)diff);
            int grid[MAX_CELLS];
            int cells[MAX_CELLS];   /* open cells where fruit can go */
            Graph graph;
            Features f;
            int goal_r = level.rows - 2;
            int goal_c = level.cols - 2;

            graph_init(&graph, level.rows, level.cols);
            if (!generate_maze(grid, level.rows, level.cols, level.loop_percent, (unsigned int)seed) ||
                !build_graph(&graph, grid, level.rows, level.cols) ||
                !get_features(&graph, 1, 1, goal_r, goal_c, &f))
            {
                graph_free(&graph);
                fprintf(stderr, "Failed maze seed=%lu difficulty=%d\n", seed, diff);
                return 1;
            }

            int start = vertex_at(&graph, 1, 1);
            int goal = vertex_at(&graph, goal_r, goal_c);

            /* all open cells except start and goal */
            int count = 0;
            for (i = 0; i < graph.vertex_count; i++)
            {
                if (i != start && i != goal)
                {
                    cells[count] = i;
                    count++;
                }
            }

            /* shuffle them; the first fruit_count cells get fruit.
               the 0x9e3779b9 just mixes the seed so each difficulty is different */
            uint32_t rng = (uint32_t)seed ^ ((uint32_t)(diff + 1) * 0x9e3779b9u);
            for (i = count - 1; i > 0; i--)
            {
                int j = next_random(&rng) % (i + 1);
                int temp = cells[i];
                cells[i] = cells[j];
                cells[j] = temp;
            }

            /* run both bots. every trial uses the same fruit places */
            int wins[2] = {0, 0};
            for (bot = 0; bot < 2; bot++)
            {
                unsigned long t;
                for (t = 0; t < trials; t++)
                {
                    int result = run_bot(&graph, start, goal, cells, level.fruit_count,
                                         bot, next_random(&rng));
                    if (result < 0)
                    {
                        graph_free(&graph);
                        return 1;
                    }
                    wins[bot] = wins[bot] + result;
                }
            }

            double success_rate = (wins[0] + wins[1]) / (2.0 * trials);
            printf("%lu,%d,%d,%lu,%d,%d,%d,%d,%d,%d,%.17g,%d,%d,%.17g\n",
                   seed, diff, level.fruit_count, trials,
                   f.vertices, f.edges, f.dead_ends, f.junctions, f.loops,
                   f.shortest_path, f.detour_ratio, wins[0], wins[1], success_rate);

            graph_free(&graph);
        }
    }

    if (ferror(stdout))   /* e.g. disk full */
        return 1;
    return 0;
}
