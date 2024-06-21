#include "optics.h"

void *sceneA_init();
void *sceneB_init();
void *sceneC_init();
void *sceneD_init();

void sceneA_draw(void*);
void sceneB_draw(void*);
void sceneC_draw(void*);
void sceneD_draw(void*);

#define NSCENES 4
int main() {
   	const int display = GetCurrentMonitor();
    const int screenWidth = GetMonitorWidth(display);
    const int screenHeight = GetMonitorHeight(display);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Optics");

    void *scene_data[NSCENES] = {sceneA_init(), sceneB_init(), sceneC_init(), sceneD_init() };
    std::function<void(void*)> scene_fns[NSCENES] = { sceneA_draw, sceneB_draw, sceneC_draw, sceneD_draw };
    int ix2Scene[NSCENES] = { 0, 1, 2, 3 };
    int ix = NSCENES - 1;
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_TAB)) {
            if (IsKeyDown(KEY_LEFT_SHIFT)) {
              ix = (ix - 1);
              if (ix < 0) { ix = NSCENES - 1; };
            } else {
              ix = (ix + 1);
              if (ix == NSCENES) { ix = 0; }
            }
        }

        int scene = ix2Scene[ix];
        scene_fns[scene](scene_data[scene]);

    }

    CloseWindow(); 
    return 0;
}
