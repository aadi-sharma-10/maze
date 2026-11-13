/* menu.c - login / sign up screen (raylib + SQLite). Not linked to the game yet. */
#include "raylib.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720
#define MAX_LEN 64
#define DB_FILE "maze_game.db"

/* which text box is selected */
#define FIELD_NONE 0
#define FIELD_USER 1
#define FIELD_PASS 2

/* turns the password into a number (FNV-1a hash) so we don't save the real password.
   note: this is a simple hash, not safe enough for a real app */
void HashPassword(const char *password, char hex[32])
{
    unsigned long long hash = 14695981039346656037ULL;
    int i;
    for (i = 0; password[i] != '\0'; i++)
    {
        hash = hash ^ (unsigned char)password[i];
        hash = hash * 1099511628211ULL;
    }
    snprintf(hex, 32, "%016llx", hash);
}

/* draws a text box. if hide is true it shows **** instead of the text */
void DrawInputBox(Rectangle box, const char *text, bool selected, bool hide)
{
    char shown[MAX_LEN + 1];
    int i;

    if (selected)
    {
        DrawRectangleRec(box, (Color){40, 40, 60, 255});
        DrawRectangleLinesEx(box, 2, SKYBLUE);
    }
    else
    {
        DrawRectangleRec(box, (Color){30, 30, 30, 255});
        DrawRectangleLinesEx(box, 2, GRAY);
    }

    strcpy(shown, text);
    if (hide)
    {
        for (i = 0; shown[i] != '\0'; i++)
            shown[i] = '*';
    }

    DrawText(shown, box.x + 10, box.y + box.height / 2 - 10, 20, WHITE);
}

/* adds typed letters to the text and handles backspace */
void UpdateTextField(char *text)
{
    int len = strlen(text);
    int key = GetCharPressed();

    while (key > 0)
    {
        if (key >= 32 && key <= 125 && len < MAX_LEN - 1)
        {
            text[len] = (char)key;
            len++;
            text[len] = '\0';
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && len > 0)
        text[len - 1] = '\0';
}

bool DrawButton(Rectangle box, const char *text, Color color)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), box);
    int fontSize = 22;

    if (hover)
        DrawRectangleRec(box, color);
    else
        DrawRectangleRec(box, Fade(color, 0.7f));
    DrawRectangleLinesEx(box, 2, WHITE);

    DrawText(text, box.x + box.width / 2 - MeasureText(text, fontSize) / 2,
             box.y + box.height / 2 - fontSize / 2, fontSize, WHITE);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* saves a new user. writes the result in message */
void Signup(const char *username, const char *password, char *message)
{
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    char hash[32];

    if (strlen(username) == 0 || strlen(password) == 0)
    {
        strcpy(message, "Username/password can't be empty! X");
        return;
    }

    if (sqlite3_open(DB_FILE, &db) != SQLITE_OK)
    {
        strcpy(message, "Database connection failed! X");
        sqlite3_close(db);
        return;
    }

    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS users("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "username TEXT UNIQUE, password TEXT);", NULL, NULL, NULL);

    HashPassword(password, hash);

    /* ? are filled in with bind_text, this stops SQL injection */
    if (sqlite3_prepare_v2(db, "INSERT INTO users (username, password) VALUES (?, ?);",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        strcpy(message, "Database Query Error! X");
    }
    else
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
            strcpy(message, "Account created! You can log in now.");
        else
            strcpy(message, "Username already exists! X");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

/* checks username and password. writes the result in message */
void Login(const char *username, const char *password, char *message)
{
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    char hash[32];

    if (sqlite3_open(DB_FILE, &db) != SQLITE_OK)
    {
        strcpy(message, "Database connection failed! X");
        sqlite3_close(db);
        return;
    }

    HashPassword(password, hash);

    if (sqlite3_prepare_v2(db, "SELECT * FROM users WHERE username = ? AND password = ?;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        strcpy(message, "Database Query Error! X");
    }
    else
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
            strcpy(message, "Login successful!");
        else
            strcpy(message, "Wrong username or password! X");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Maze - Login");
    SetTargetFPS(60);

    Texture2D background = LoadTexture("assets/background.png");

    bool signupMode = true;    /* false = login mode */
    int selected = FIELD_NONE;
    char username[MAX_LEN] = "";
    char password[MAX_LEN] = "";
    char message[128] = "";

    Rectangle panel = {SCREEN_WIDTH / 2 - 220, SCREEN_HEIGHT / 2 - 200, 440, 400};
    Rectangle signupTab = {panel.x, panel.y, panel.width / 2, 50};
    Rectangle loginTab = {panel.x + panel.width / 2, panel.y, panel.width / 2, 50};
    Rectangle userBox = {panel.x + 30, panel.y + 100, panel.width - 60, 45};
    Rectangle passBox = {panel.x + 30, panel.y + 170, panel.width - 60, 45};
    Rectangle submitButton = {panel.x + 30, panel.y + 240, panel.width - 60, 50};
    Color tabColor = {40, 40, 50, 255};

    while (!WindowShouldClose())
    {
        /* ----- input ----- */
        Vector2 mouse = GetMousePosition();
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        if (clicked)
        {
            if (CheckCollisionPointRec(mouse, userBox))
                selected = FIELD_USER;
            else if (CheckCollisionPointRec(mouse, passBox))
                selected = FIELD_PASS;
            else
                selected = FIELD_NONE;

            if (CheckCollisionPointRec(mouse, signupTab))
            {
                signupMode = true;
                message[0] = '\0';
            }
            if (CheckCollisionPointRec(mouse, loginTab))
            {
                signupMode = false;
                message[0] = '\0';
            }
        }

        if (IsKeyPressed(KEY_TAB))
        {
            if (selected == FIELD_USER)
                selected = FIELD_PASS;
            else
                selected = FIELD_USER;
        }

        if (selected == FIELD_USER)
            UpdateTextField(username);
        else if (selected == FIELD_PASS)
            UpdateTextField(password);

        /* ----- draw ----- */
        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexturePro(background, (Rectangle){0, 0, background.width, background.height},
                       (Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, (Vector2){0, 0}, 0, WHITE);

        DrawRectangleRec(panel, (Color){25, 25, 35, 180});
        DrawRectangleLinesEx(panel, 2, GRAY);

        /* tabs */
        DrawRectangleRec(signupTab, signupMode ? SKYBLUE : tabColor);
        DrawRectangleRec(loginTab, signupMode ? tabColor : SKYBLUE);
        DrawText("SIGN UP", signupTab.x + 20, signupTab.y + 15, 20, WHITE);
        DrawText("LOG IN", loginTab.x + 30, loginTab.y + 15, 20, WHITE);

        DrawText("Username:", userBox.x, userBox.y - 22, 16, LIGHTGRAY);
        DrawInputBox(userBox, username, selected == FIELD_USER, false);
        DrawText("Password:", passBox.x, passBox.y - 22, 16, LIGHTGRAY);
        DrawInputBox(passBox, password, selected == FIELD_PASS, true);

        if (DrawButton(submitButton, signupMode ? "CREATE ACCOUNT" : "LOG IN", (Color){70, 170, 255, 255}))
        {
            if (signupMode)
                Signup(username, password, message);
            else
                Login(username, password, message);
        }

        if (message[0] != '\0')
            DrawText(message, panel.x + 30, submitButton.y + 70, 18, YELLOW);

        EndDrawing();
    }

    UnloadTexture(background);
    CloseWindow();
    return 0;
}
