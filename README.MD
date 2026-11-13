# MazeQuest

A maze game made with C and C++. The maze is generated in C, stored as a graph
(adjacency list), and checked with BFS (queue) and DFS (stack). The game screen
is written in C++ with classes. We also trained a small model that guesses how
hard a maze is.

## Files

```text
include/maze.h          all structs and function declarations
src/maze_generator.c    makes a random maze (DFS backtracker) + terminal version
src/maze_core.c         graph, BFS, DFS, maze features, the two bots, stamina rules
src/maze_game.cpp       the game (C++ classes: Maze, Player, Fruit, Game)
src/menu.c              login / sign up screen with SQLite (not linked to the game yet)
src/maze_dataset.c      makes the training data, and --score runs the model
tests/test_all.c        C tests
tests/train_model_test.py  Python tests
tools/train_model.py    trains the model and checks C gives the same answers
data/maze_data.csv      the training data
model/                  trained model (maze_model.h for C, .json, report.json)
assets/                 pictures used by the game and menu
```

## How to run

Run every command from this folder (the one with the `Makefile`), otherwise the
game can't find the pictures in `assets/`.

```sh
make game && ./build/maze_game     # the game
make cli && ./build/maze_cli       # maze in the terminal (no raylib needed)
make menu && ./build/menu          # login / sign up screen
make test                          # all tests
make train                         # make the data again and retrain the model
```

**Controls:** pick EASY / MEDIUM / HARD, move with WASD or arrow keys. Every
move costs 1 stamina, each fruit gives +20 (max 100). Reach the green circle
before stamina runs out. Press Enter after the game to go back to the menu.

### What you need

| Program | Needs |
| --- | --- |
| Game | C and C++ compiler, make, raylib |
| Terminal maze | C compiler, make |
| Login menu | C compiler, make, raylib, SQLite (`libsqlite3-dev` on Linux) |
| Tests / training | C compiler, make, Python 3.10+ (no extra packages) |

Raylib from Homebrew (`/opt/homebrew`) is found automatically on Mac.
On Windows use the MSYS2 UCRT64 terminal:

```sh
make game RAYLIB_LIBS='-lraylib -lopengl32 -lgdi32 -lwinmm'
./build/maze_game.exe
```

If `python3` is called `python` on your computer: `make test PYTHON=python`.

## How the maze works

1. `generate_maze()` starts with all walls and does a DFS from cell (1,1),
   breaking walls to random unvisited neighbours. Easy and medium mazes then
   break some extra walls (15% / 5%) so there are loops.
2. `build_graph()` turns every open cell into a vertex and joins neighbours
   with edges (adjacency list).
3. `bfs_shortest_path()` finds the shortest path to the exit and `dfs_count()`
   counts reachable cells. The game only starts if the exit can be reached.

| Difficulty | Size | Loops | Fruits |
| --- | --- | --- | --- |
| Easy | 15 x 15 | 15% | 5 |
| Medium | 21 x 21 | 5% | 4 |
| Hard | 31 x 31 | 0% | 3 |

## Difficulty model

To guess how hard a maze is, two bots play each maze with the same stamina
rules as the player:

- **random bot**: goes to any neighbour
- **greedy bot**: goes to the neighbour it visited least, and if tied, the one
  closest to the exit (Manhattan distance)

`make train` makes 600 mazes (seeds 1-200, all 3 difficulties), runs each bot
64 times per maze, and saves the success rate with 7 features of the maze:
vertices, edges, dead ends, junctions, loops, shortest path length (moves),
and detour ratio (shortest path / straight line distance).

`tools/train_model.py` fits a ridge regression (only the Python standard
library). Every 5th seed is kept for testing, with all its difficulties, so the
same maze seed is never in both train and test. We compare it with a baseline
that only uses `shortest_path + dead_ends`.

**Result:** test MSE **0.01358** for the model vs **0.01524** for the baseline
(about 10.9% better). The C version of the model (`model/maze_model.h`) gives
the same answers as Python within 1e-12 on all 600 mazes. This only measures
these two bots, not real players.

Using the model from C:

```c
#include "maze.h"
#include "maze_model.h"    /* compile with -Iinclude -Imodel */

Features f;
if (get_features(&graph, 1, 1, rows - 2, cols - 2, &f)) {
    double arr[FEATURE_COUNT];
    features_to_array(&f, arr);
    double success_rate = predict_success(arr);   /* 0 to 1 */
}
```

To try a different experiment without changing the saved files:

```sh
./build/maze_dataset 100 32 1001 > build/custom.csv
python3 tools/train_model.py build/custom.csv --output build/custom_model
```

## Common problems

- **Pictures missing:** run `./build/maze_game` from this folder, not from inside `build/`.
- **`raylib.h` not found:** install raylib, or build with
  `make game RAYLIB_CFLAGS=-I/path/to/include RAYLIB_LIBS='-L/path/to/lib -lraylib'`.

## Credits

- Fruit icons: https://scrimsy.itch.io/fruit-icons-16x16
- Raylib: https://www.raylib.com/
