#include "raylib.h"

#include "maze_generator.h"
#include "maze_graph.h"
#include "maze_rules.h"

#include <algorithm>
#include <ctime>
#include <vector>

namespace {

constexpr int screenWidth = 1280;
constexpr int screenHeight = 720;
constexpr int maxStamina = MAZE_STAMINA_MAX;

Vector2 CellCenter(int row, int col, int offsetX, int offsetY, int tileSize)
{
    return {
        static_cast<float>(offsetX + col * tileSize + tileSize / 2),
        static_cast<float>(offsetY + row * tileSize + tileSize / 2)
    };
}

} // namespace

enum class Screen {
    Menu,
    Playing
};

bool DrawButton(Rectangle rect, const char *text)
{
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, rect);

    DrawRectangleRec(rect, hover ? Color{80, 150, 210, 255} : Color{50, 95, 145, 255});
    DrawRectangleLinesEx(rect, 2, RAYWHITE);

    int fontSize = 24;
    int textWidth = MeasureText(text, fontSize);
    DrawText(text,
             static_cast<int>(rect.x + rect.width / 2 - textWidth / 2),
             static_cast<int>(rect.y + rect.height / 2 - fontSize / 2),
             fontSize,
             RAYWHITE);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

class Maze {
public:
    Maze()
    {
        MazeGraph_Init(&graph_, 1, 1);
    }

    ~Maze()
    {
        MazeGraph_Free(&graph_);
    }

    Maze(const Maze&) = delete;
    Maze& operator=(const Maze&) = delete;

    // Makes a new random maze. Returns false if it could not be built or solved.
    bool Generate(MazeDifficulty difficulty)
    {
        MazeDifficultyParams params;
        MazeGen_GetParams(difficulty, &params);

        rows_ = params.rows;
        cols_ = params.cols;
        fruitCount_ = params.fruit_count;
        grid_.assign(rows_ * cols_, 1);
        pathLength_ = 0;
        reachableCount_ = 0;

        unsigned int seed = static_cast<unsigned int>(GetRandomValue(1, 1000000000));
        if (!MazeGen_Generate(grid_.data(), rows_, cols_, params.loop_percent, seed))
            return false;
        if (!MazeGraph_BuildFromGrid(&graph_, grid_.data(), rows_, cols_))
            return false;

        std::vector<int> path(MAZE_GRAPH_MAX_CELLS);
        pathLength_ = MazeGraph_FindShortestPath(&graph_, StartRow(), StartCol(),
                                                 GoalRow(), GoalCol(),
                                                 path.data(), static_cast<int>(path.size()));
        reachableCount_ = MazeGraph_DfsReachableCount(&graph_, StartRow(), StartCol());
        return HasSolution();
    }

    bool IsWall(int row, int col) const
    {
        return grid_[row * cols_ + col] == 1;
    }

    bool HasSolution() const
    {
        return pathLength_ > 0 && reachableCount_ > 0;
    }

    int Rows() const { return rows_; }
    int Cols() const { return cols_; }
    int FruitCount() const { return fruitCount_; }

    // The generator always starts at (1, 1) and ends in the opposite corner.
    int StartRow() const { return 1; }
    int StartCol() const { return 1; }
    int GoalRow() const { return rows_ - 2; }
    int GoalCol() const { return cols_ - 2; }

private:
    MazeGraph graph_{};
    std::vector<int> grid_;
    int rows_ = 0;
    int cols_ = 0;
    int fruitCount_ = 0;
    int pathLength_ = 0;
    int reachableCount_ = 0;
};

class Player {
public:
    int row = 0;
    int col = 0;
    int stamina = maxStamina;

    void Reset(const Maze& maze)
    {
        row = maze.StartRow();
        col = maze.StartCol();
        stamina = maxStamina;
    }

    bool TryMove(int dRow, int dCol, const Maze& maze)
    {
        int nextRow = row + dRow;
        int nextCol = col + dCol;
        if (nextRow < 0 || nextRow >= maze.Rows() || nextCol < 0 || nextCol >= maze.Cols())
            return false;
        if (maze.IsWall(nextRow, nextCol) || stamina <= 0)
            return false;

        row = nextRow;
        col = nextCol;
        stamina = MazeRules_Move(stamina);
        return true;
    }
};

class Fruit {
public:
    Fruit(int row, int col, Texture2D icon)
        : row_(row), col_(col), icon_(icon) {}

    int Row() const { return row_; }
    int Col() const { return col_; }

    void Draw(int offsetX, int offsetY, int tileSize) const
    {
        if (!collected_)
        {
            Vector2 center = CellCenter(row_, col_, offsetX, offsetY, tileSize);
            float size = tileSize * 0.8f;
            Rectangle target = {center.x - size / 2, center.y - size / 2, size, size};

            if (icon_.id != 0)
            {
                DrawTexturePro(icon_,
                    Rectangle{0, 0, static_cast<float>(icon_.width), static_cast<float>(icon_.height)},
                    target,
                    Vector2{0, 0},
                    0,
                    WHITE);
            }
            else
            {
                DrawCircleV(center, size / 3, ORANGE);
            }
        }
    }

    void TryCollect(Player& player)
    {
        if (!collected_ && player.row == row_ && player.col == col_)
        {
            collected_ = true;
            player.stamina = MazeRules_Collect(player.stamina);
        }
    }

private:
    int row_;
    int col_;
    Texture2D icon_;
    bool collected_ = false;
};

class Game {
public:
    Game()
    {
        InitWindow(screenWidth, screenHeight, "MazeQuest - C and C++ Hybrid");
        SetTargetFPS(60);
        SetRandomSeed(static_cast<unsigned int>(std::time(nullptr)));

        background_ = LoadTexture("assets/game_background.png");
        shrub_ = LoadTexture("assets/bush.png");
        playerTexture_ = LoadTexture("assets/player.png");

        fruitIcons_[0] = LoadTexture("assets/apple.png");
        fruitIcons_[1] = LoadTexture("assets/banana.png");
        fruitIcons_[2] = LoadTexture("assets/grapes.png");
        fruitIcons_[3] = LoadTexture("assets/strawberry.png");
    }

    ~Game()
    {
        for (Texture2D& icon : fruitIcons_)
            UnloadTexture(icon);

        UnloadTexture(shrub_);
        UnloadTexture(background_);
        UnloadTexture(playerTexture_);
        CloseWindow();
    }

    void Run()
    {
        while (!WindowShouldClose() && !quit_)
        {
            Update();
            Draw();
        }
    }

private:
    void Update()
    {
        if (screen_ == Screen::Menu)
        {
            return;
        }

        if (won_ || lost_)
        {
            if (IsKeyPressed(KEY_ENTER))
                screen_ = Screen::Menu;
            return;
        }

        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) player_.TryMove(-1, 0, maze_);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) player_.TryMove(1, 0, maze_);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) player_.TryMove(0, -1, maze_);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) player_.TryMove(0, 1, maze_);

        for (Fruit& fruit : fruits_)
            fruit.TryCollect(player_);

        won_ = player_.row == maze_.GoalRow() && player_.col == maze_.GoalCol();
        lost_ = player_.stamina == 0 && !won_;
    }

    void Draw()
    {
        BeginDrawing();
        ClearBackground(BLACK);

        DrawBackground();

        if (screen_ == Screen::Menu)
        {
            DrawMenu();
        }
        else
        {
            DrawMaze();
            DrawHud();

            if (won_) DrawText("YOU ESCAPED!", 515, 330, 40, GREEN);
            if (lost_) DrawText("STAMINA EMPTY", 500, 330, 40, RED);
            if (won_ || lost_) DrawText("Press ENTER for menu", 520, 380, 24, RAYWHITE);
        }


        EndDrawing();
    }

    void DrawMenu()
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 120});
        DrawText("MazeQuest", 505, 170, 54, RAYWHITE);

        Rectangle easyButton = {520, 280, 240, 58};
        Rectangle mediumButton = {520, 350, 240, 58};
        Rectangle hardButton = {520, 420, 240, 58};
        Rectangle quitButton = {520, 490, 240, 58};

        if (DrawButton(easyButton, "EASY"))
            StartGame(MAZE_DIFFICULTY_EASY);

        if (DrawButton(mediumButton, "MEDIUM"))
            StartGame(MAZE_DIFFICULTY_MEDIUM);

        if (DrawButton(hardButton, "HARD"))
            StartGame(MAZE_DIFFICULTY_HARD);

        if (DrawButton(quitButton, "QUIT"))
            quit_ = true;

        if (mazeFailed_)
            DrawText("Could not make a valid maze. Try again.", 460, 570, 20, RED);
    }

    void DrawBackground()
    {
        if (background_.id != 0)
        {
            DrawTexturePro(background_,
                Rectangle{0, 0, static_cast<float>(background_.width), static_cast<float>(background_.height)},
                Rectangle{0, 0, screenWidth, screenHeight},
                Vector2{0, 0}, 0, WHITE);
        }
        else
        {
            ClearBackground(Color{22, 34, 29, 255});
        }
    }

    void DrawMaze()
    {
        for (int row = 0; row < maze_.Rows(); row++)
        {
            for (int col = 0; col < maze_.Cols(); col++)
            {
                Rectangle tile = {
                    static_cast<float>(offsetX_ + col * tileSize_),
                    static_cast<float>(offsetY_ + row * tileSize_),
                    static_cast<float>(tileSize_),
                    static_cast<float>(tileSize_)
                };

                if (maze_.IsWall(row, col))
                {
                    if (shrub_.id != 0)
                    {
                        DrawTexturePro(shrub_,
                            Rectangle{0, 0, static_cast<float>(shrub_.width), static_cast<float>(shrub_.height)},
                            tile, Vector2{0, 0}, 0, WHITE);
                    }
                    else
                    {
                        DrawRectangleRec(tile, DARKGREEN);
                    }
                }
                else
                {
                    DrawRectangleRec(tile, Color{12, 14, 18, 115});
                }
            }
        }

        for (const Fruit& fruit : fruits_)
            fruit.Draw(offsetX_, offsetY_, tileSize_);

        DrawPlayer();
        DrawCircleV(CellCenter(maze_.GoalRow(), maze_.GoalCol(), offsetX_, offsetY_, tileSize_),
                    tileSize_ / 3.0f, LIME);
    }

    void DrawPlayer()
    {
        Vector2 center = CellCenter(player_.row, player_.col, offsetX_, offsetY_, tileSize_);
        float size = static_cast<float>(tileSize_ - 2);
        Rectangle target = {center.x - size / 2, center.y - size / 2, size, size};

        if (playerTexture_.id != 0)
        {
            DrawTexturePro(playerTexture_,
                Rectangle{0, 0, static_cast<float>(playerTexture_.width), static_cast<float>(playerTexture_.height)},
                target,
                Vector2{0, 0},
                0,
                WHITE);
        }
        else
        {
            DrawCircleV(center, size / 3, SKYBLUE);
        }
    }

    void DrawHud()
    {
        DrawText("Stamina", 36, 30, 20, RAYWHITE);
        DrawRectangle(36, 58, 240, 16, DARKGRAY);
        DrawRectangle(36, 58, static_cast<int>(240 * (player_.stamina / static_cast<float>(maxStamina))), 16, GREEN);
        DrawText(TextFormat("%d", player_.stamina), 288, 54, 22, RAYWHITE);
    }

    // Puts the fruit on random open cells (not the start or the goal).
    void PlaceFruits()
    {
        fruits_.clear();

        while (static_cast<int>(fruits_.size()) < maze_.FruitCount())
        {
            int row = GetRandomValue(0, maze_.Rows() - 1);
            int col = GetRandomValue(0, maze_.Cols() - 1);

            if (maze_.IsWall(row, col)) continue;
            if (row == maze_.StartRow() && col == maze_.StartCol()) continue;
            if (row == maze_.GoalRow() && col == maze_.GoalCol()) continue;

            bool taken = false;
            for (const Fruit& fruit : fruits_)
            {
                if (fruit.Row() == row && fruit.Col() == col)
                    taken = true;
            }
            if (taken) continue;

            fruits_.push_back(Fruit(row, col, fruitIcons_[fruits_.size() % 4]));
        }
    }

    void StartGame(MazeDifficulty difficulty)
    {
        mazeFailed_ = !maze_.Generate(difficulty);
        if (mazeFailed_) return;

        // Fit the maze on screen and centre it.
        tileSize_ = (screenHeight - 40) / maze_.Rows();
        offsetX_ = (screenWidth - maze_.Cols() * tileSize_) / 2;
        offsetY_ = (screenHeight - maze_.Rows() * tileSize_) / 2;

        player_.Reset(maze_);
        PlaceFruits();

        won_ = false;
        lost_ = false;
        screen_ = Screen::Playing;
    }

    Maze maze_;
    Player player_;
    int tileSize_ = 30;
    int offsetX_ = 0;
    int offsetY_ = 0;
    Texture2D background_{};
    Texture2D shrub_{};
    Texture2D playerTexture_{};
    Texture2D fruitIcons_[4]{};
    std::vector<Fruit> fruits_;
    Screen screen_ = Screen::Menu;
    bool won_ = false;
    bool lost_ = false;
    bool mazeFailed_ = false;
    bool quit_ = false;
};

int main()
{
    Game game;
    game.Run();
    return 0;
}
