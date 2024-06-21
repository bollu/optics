#include "optics.h"

void *scene1_init();
void *scene2_init();

void scene1_draw(void*);
void scene2_draw(void*);

#define NSCENES 2
int main() {
   	const int display = GetCurrentMonitor();
    const int screenWidth = GetMonitorWidth(display);
    const int screenHeight = GetMonitorHeight(display);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Optics");

    void *data[NSCENES];
    data[0] = scene1_init();
    data[1] = scene2_init();
    int cur_scene = 0;

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_TAB)) {
            cur_scene = (cur_scene + 1) % NSCENES;
        }

        if (cur_scene == 0) {
            scene1_draw(data[0]);
        }
        else if (cur_scene == 1) {
            scene2_draw(data[1]);
        } else {
            assert(false && "cur_scene must be < NSCENES");
        }

    }

    CloseWindow(); 
    return 0;
}
