#include "raylib.h"
#include <stdlib.h>
#include <assert.h>

typedef struct {
    Vector2 ballPosition;
    Vector2 ballSpeed;
    int ballRadius;
    bool pause;
    int framesCounter;
} Scene1Data;

void* scene1_init(void) {
    Scene1Data *data = malloc(sizeof(Scene1Data));
    data->ballPosition.x = GetScreenWidth()/2.0f;
    data->ballPosition.y = GetScreenHeight()/2.0f;
    data->ballSpeed.x = 5.0;
    data->ballSpeed.y = 4.0;
    data->ballRadius = 20;
    data->pause = 0;
    data->framesCounter = 0;
    return data;
}

int scene1_draw(Scene1Data *data) {
    if (IsKeyPressed(KEY_SPACE)) data->pause = !data->pause;

    if (!data->pause)
    {
        data->ballPosition.x += data->ballSpeed.x;
        data->ballPosition.y += data->ballSpeed.y;

        // Check walls collision for bouncing
        if ((data->ballPosition.x >= (GetScreenWidth() - data->ballRadius)) || (data->ballPosition.x <= data->ballRadius)) data->ballSpeed.x *= -1.0f;
        if ((data->ballPosition.y >= (GetScreenHeight() - data->ballRadius)) || (data->ballPosition.y <= data->ballRadius)) data->ballSpeed.y *= -1.0f;
    }
    else { data->framesCounter++; }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawCircleV(data->ballPosition, (float)data->ballRadius, MAROON);
    //DrawText("PRESS SPACE to PAUSE BALL MOVEMENT", 10, GetScreenHeight() - 25, 20, LIGHTGRAY);

    // On pause, we draw a blinking message
    if (data->pause && ((data->framesCounter/30)%2)) DrawText("PAUSED", 350, 200, 30, GRAY);

    DrawFPS(10, 10);
    EndDrawing();
}

#define NSCENES 1
int main() {
    const int screenWidth = 800;
    const int screenHeight = 450;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Optics");

    void *data[NSCENES];
    data[0] = scene1_init();
    int cur_scene = 0;

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_TAB)) {
            cur_scene = (cur_scene + 1) % NSCENES;
        }
        if (cur_scene == 0) {
            scene1_draw((Scene1Data*)data[cur_scene]);

        } else {
            assert(false && "cur_scene must be < NSCENES");
        }

    }

    CloseWindow(); 
    return 0;
}