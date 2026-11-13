// maze_game.cpp - the playable game (C++ classes + raylib).
// The maze itself is made by the C code in maze_generator.c and maze_core.c
#include "raylib.h"
#include "maze.h"
#include <ctime>
#include <vector>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// middle of a cell on the screen
Vector2 CellCenter(int row, int col, int offsetX, int offsetY, int tileSize)
{
    Vector2 v;
    v.x = (float)(offsetX + col * tileSize + tileSize / 2);
    v.y = (float)(offsetY + row * tileSize + tileSize / 2);
    return v;
}

// draws a picture stretched into a box
void DrawImageInBox(Texture2D tex, Rectangle box)
{
    Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
    DrawTexturePro(tex, source, box, Vector2{0, 0}, 0, WHITE);
}

// draws a button and returns true when it is clicked
bool DrawButton(Rectangle rect, const char *text)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), rect);

    if (hover)
        DrawRectangleRec(rect, Color{80, 150, 210, 255});
    else
        DrawRectangleRec(rect, Color{50, 95, 145, 255});
    DrawRectangleLinesEx(rect, 2, RAYWHITE);

    int fontSize = 24;
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, (int)(rect.x + rect.width / 2 - textWidth / 2),
             (int)(rect.y + rect.height / 2 - fontSize / 2), fontSize, RAYWHITE);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// ------------------------------------------------------------------
class Maze {
public:
    int rows;
    int cols;
    int fruitCount;
    int grid[MAX_CELLS];   // 1 = wall, 0 = path
    Graph graph;

    Maze()
    {
        rows = 0;
        cols = 0;
        fruitCount = 0;
        graph_init(&graph, 1, 1);
    }

    ~Maze()
    {
        graph_free(&graph);
    }

    // makes a new random maze. returns false if it can't be solved
    bool Generate(Difficulty difficulty)
    {
        Level level = get_level(difficulty);
        rows = level.rows;
        cols = level.cols;
        fruitCount = level.fruit_count;

        unsigned int seed = (unsigned int)GetRandomValue(1, 1000000000);
        if (!generate_maze(grid, rows, cols, level.loop_percent, seed))
            return false;
        if (!build_graph(&graph, grid, rows, cols))
            return false;

        // check with BFS and DFS that the exit can be reached
        int path[MAX_CELLS];
        int pathLength = bfs_shortest_path(&graph, StartRow(), StartCol(),
                                           GoalRow(), GoalCol(), path, MAX_CELLS);
        int reachable = dfs_count(&graph, StartRow(), StartCol());
        return pathLength > 0 && reachable > 0;
    }

    bool IsWall(int row, int col)
    {
        return grid[row * cols + col] == 1;
    }

    // start is always (1,1) and the exit is the opposite corner
    int StartRow() { return 1; }
    int StartCol() { return 1; }
    int GoalRow() { return rows - 2; }
    int GoalCol() { return cols - 2; }
};

// ------------------------------------------------------------------
class Player {
public:
    int row;
    int col;
    int stamina;

    Player()
    {
        row = 0;
        col = 0;
        stamina = STAMINA_MAX;
    }

    void Reset(Maze &maze)
    {
        row = maze.StartRow();
        col = maze.StartCol();
        stamina = STAMINA_MAX;
    }

    void TryMove(int dRow, int dCol, Maze &maze)
    {
        int nextRow = row + dRow;
        int nextCol = col + dCol;

        if (nextRow < 0 || nextRow >= maze.rows || nextCol < 0 || nextCol >= maze.cols)
            return;
        if (maze.IsWall(nextRow, nextCol))
            return;
        if (stamina <= 0)
            return;

        row = nextRow;
        col = nextCol;
        stamina = stamina_after_move(stamina);
    }
};

// ------------------------------------------------------------------
class Fruit {
public:
    int row;
    int col;
    bool collected;
    Texture2D icon;

    Fruit(int r, int c, Texture2D picture)
    {
        row = r;
        col = c;
        collected = false;
        icon = picture;
    }

    void Draw(int offsetX, int offsetY, int tileSize)
    {
        if (collected)
            return;

        Vector2 center = CellCenter(row, col, offsetX, offsetY, tileSize);
        float size = tileSize * 0.8f;
        Rectangle box = {center.x - size / 2, center.y - size / 2, size, size};

        if (icon.id != 0)
            DrawImageInBox(icon, box);
        else
            DrawCircleV(center, size / 3, ORANGE);   // picture not found
    }

    void TryCollect(Player &player)
    {
        if (!collected && player.row == row && player.col == col)
        {
            collected = true;
            player.stamina = stamina_after_fruit(player.stamina);
        }
    }
};

// ------------------------------------------------------------------
class Game {
public:
    Maze maze;
    Player player;
    std::vector<Fruit> fruits;

    Texture2D background;
    Texture2D bush;
    Texture2D playerPicture;
    Texture2D fruitPictures[4];

    int tileSize;
    int offsetX;
    int offsetY;

    bool inMenu;
    bool won;
    bool lost;
    bool mazeFailed;
    bool quit;

    Game()
    {
        InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "MazeQuest - C and C++ Hybrid");
        SetTargetFPS(60);
        SetRandomSeed((unsigned int)time(NULL));

        background = LoadTexture("assets/game_background.png");
        bush = LoadTexture("assets/bush.png");
        playerPicture = LoadTexture("assets/player.png");
        fruitPictures[0] = LoadTexture("assets/apple.png");
        fruitPictures[1] = LoadTexture("assets/banana.png");
        fruitPictures[2] = LoadTexture("assets/grapes.png");
        fruitPictures[3] = LoadTexture("assets/strawberry.png");

        tileSize = 30;
        offsetX = 0;
        offsetY = 0;
        inMenu = true;
        won = false;
        lost = false;
        mazeFailed = false;
        quit = false;
    }

    ~Game()
    {
        for (int i = 0; i < 4; i++)
            UnloadTexture(fruitPictures[i]);
        UnloadTexture(bush);
        UnloadTexture(background);
        UnloadTexture(playerPicture);
        CloseWindow();
    }

    void Run()
    {
        while (!WindowShouldClose() && !quit)
        {
            Update();
            Draw();
        }
    }

    void StartGame(Difficulty difficulty)
    {
        mazeFailed = !maze.Generate(difficulty);
        if (mazeFailed)
            return;

        // make the maze fit on the screen, in the middle
        tileSize = (SCREEN_HEIGHT - 40) / maze.rows;
        offsetX = (SCREEN_WIDTH - maze.cols * tileSize) / 2;
        offsetY = (SCREEN_HEIGHT - maze.rows * tileSize) / 2;

        player.Reset(maze);
        PlaceFruits();

        won = false;
        lost = false;
        inMenu = false;
    }

    // put fruit on random open cells (not on start or exit, not two on one cell)
    void PlaceFruits()
    {
        fruits.clear();

        while ((int)fruits.size() < maze.fruitCount)
        {
            int row = GetRandomValue(0, maze.rows - 1);
            int col = GetRandomValue(0, maze.cols - 1);

            if (maze.IsWall(row, col))
                continue;
            if (row == maze.StartRow() && col == maze.StartCol())
                continue;
            if (row == maze.GoalRow() && col == maze.GoalCol())
                continue;

            bool taken = false;
            for (int i = 0; i < (int)fruits.size(); i++)
            {
                if (fruits[i].row == row && fruits[i].col == col)
                    taken = true;
            }
            if (taken)
                continue;

            fruits.push_back(Fruit(row, col, fruitPictures[fruits.size() % 4]));
        }
    }

    void Update()
    {
        if (inMenu)
            return;

        if (won || lost)
        {
            if (IsKeyPressed(KEY_ENTER))
                inMenu = true;
            return;
        }

        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
            player.TryMove(-1, 0, maze);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            player.TryMove(1, 0, maze);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
            player.TryMove(0, -1, maze);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
            player.TryMove(0, 1, maze);

        for (int i = 0; i < (int)fruits.size(); i++)
            fruits[i].TryCollect(player);

        won = (player.row == maze.GoalRow() && player.col == maze.GoalCol());
        lost = (player.stamina == 0 && !won);
    }

    void Draw()
    {
        BeginDrawing();
        ClearBackground(BLACK);

        if (background.id != 0)
            DrawImageInBox(background, Rectangle{0, 0, SCREEN_WIDTH, SCREEN_HEIGHT});
        else
            ClearBackground(Color{22, 34, 29, 255});

        if (inMenu)
        {
            DrawMenu();
        }
        else
        {
            DrawMaze();
            DrawStaminaBar();

            if (won)
                DrawText("YOU ESCAPED!", 515, 330, 40, GREEN);
            if (lost)
                DrawText("STAMINA EMPTY", 500, 330, 40, RED);
            if (won || lost)
                DrawText("Press ENTER for menu", 520, 380, 24, RAYWHITE);
        }

        EndDrawing();
    }

    void DrawMenu()
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{0, 0, 0, 120});
        DrawText("MazeQuest", 505, 170, 54, RAYWHITE);

        if (DrawButton(Rectangle{520, 280, 240, 58}, "EASY"))
            StartGame(EASY);
        if (DrawButton(Rectangle{520, 350, 240, 58}, "MEDIUM"))
            StartGame(MEDIUM);
        if (DrawButton(Rectangle{520, 420, 240, 58}, "HARD"))
            StartGame(HARD);
        if (DrawButton(Rectangle{520, 490, 240, 58}, "QUIT"))
            quit = true;

        if (mazeFailed)
            DrawText("Could not make a valid maze. Try again.", 460, 570, 20, RED);
    }

    void DrawMaze()
    {
        for (int row = 0; row < maze.rows; row++)
        {
            for (int col = 0; col < maze.cols; col++)
            {
                Rectangle tile = {(float)(offsetX + col * tileSize), (float)(offsetY + row * tileSize),
                                  (float)tileSize, (float)tileSize};

                if (!maze.IsWall(row, col))
                    DrawRectangleRec(tile, Color{12, 14, 18, 115});
                else if (bush.id != 0)
                    DrawImageInBox(bush, tile);
                else
                    DrawRectangleRec(tile, DARKGREEN);
            }
        }

        for (int i = 0; i < (int)fruits.size(); i++)
            fruits[i].Draw(offsetX, offsetY, tileSize);

        // player
        Vector2 center = CellCenter(player.row, player.col, offsetX, offsetY, tileSize);
        float size = (float)(tileSize - 2);
        if (playerPicture.id != 0)
            DrawImageInBox(playerPicture, Rectangle{center.x - size / 2, center.y - size / 2, size, size});
        else
            DrawCircleV(center, size / 3, SKYBLUE);

        // exit
        DrawCircleV(CellCenter(maze.GoalRow(), maze.GoalCol(), offsetX, offsetY, tileSize),
                    tileSize / 3.0f, LIME);
    }

    void DrawStaminaBar()
    {
        int barWidth = (int)(240 * (player.stamina / (float)STAMINA_MAX));
        DrawText("Stamina", 36, 30, 20, RAYWHITE);
        DrawRectangle(36, 58, 240, 16, DARKGRAY);
        DrawRectangle(36, 58, barWidth, 16, GREEN);
        DrawText(TextFormat("%d", player.stamina), 288, 54, 22, RAYWHITE);
    }
};

int main()
{
    Game game;
    game.Run();
    return 0;
}
