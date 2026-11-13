# Run "make help"-style targets from the project folder:
#   make game | make cli | make menu | make test | make train

CC = cc
CXX = c++
PYTHON = python3
CFLAGS = -O2 -Wall -Wextra -Werror -std=c11 -Iinclude
CXXFLAGS = -O2 -Wall -Wextra -Werror -std=c++17 -Iinclude

# Raylib from Homebrew on Mac. On Windows (MSYS2) run:
#   make game RAYLIB_LIBS='-lraylib -lopengl32 -lgdi32 -lwinmm'
ifneq ($(wildcard /opt/homebrew/include/raylib.h),)
RAYLIB_CFLAGS = -I/opt/homebrew/include
RAYLIB_LIBS = -L/opt/homebrew/lib -lraylib
else
RAYLIB_CFLAGS =
RAYLIB_LIBS = -lraylib
endif

CORE = src/maze_core.c src/maze_generator.c include/maze.h

.PHONY: all game cli menu test dataset train check-model
.DELETE_ON_ERROR:

all: build/maze_dataset

build:
	mkdir -p build

build/maze_dataset: src/maze_dataset.c $(CORE) | build
	$(CC) $(CFLAGS) src/maze_dataset.c src/maze_core.c src/maze_generator.c -lm -o $@

build/maze_model_score: src/maze_model_score.c model/maze_model.h | build
	$(CC) $(CFLAGS) -Imodel src/maze_model_score.c -o $@

build/test_all: tests/test_all.c $(CORE) | build
	$(CC) $(CFLAGS) tests/test_all.c src/maze_core.c src/maze_generator.c -lm -o $@

build/maze_cli: $(CORE) | build
	$(CC) $(CFLAGS) -DMAZE_CLI src/maze_generator.c src/maze_core.c -lm -o $@

build/maze_core.o: src/maze_core.c include/maze.h | build
	$(CC) $(CFLAGS) -c src/maze_core.c -o $@

build/maze_generator.o: src/maze_generator.c include/maze.h | build
	$(CC) $(CFLAGS) -c src/maze_generator.c -o $@

# the game is C++ and links the C files compiled above
build/maze_game: src/maze_game.cpp build/maze_core.o build/maze_generator.o include/maze.h | build
	$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) src/maze_game.cpp build/maze_core.o build/maze_generator.o $(RAYLIB_LIBS) -lm -o $@

build/menu: src/menu.c | build
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) src/menu.c $(RAYLIB_LIBS) -lsqlite3 -o $@

game: build/maze_game
cli: build/maze_cli
menu: build/menu

dataset: build/maze_dataset
	mkdir -p data
	./build/maze_dataset 200 64 1 > data/maze_data.csv.tmp
	mv data/maze_data.csv.tmp data/maze_data.csv

train: dataset
	$(PYTHON) tools/train_model.py data/maze_data.csv --output model
	$(MAKE) check-model

check-model: build/maze_model_score
	$(PYTHON) tools/verify_model.py data/maze_data.csv model/maze_model.json ./build/maze_model_score

test: build/test_all build/maze_dataset
	./build/test_all
	CC="$(CC)" $(PYTHON) -m unittest discover -s tests -p '*_test.py'
	$(MAKE) check-model
