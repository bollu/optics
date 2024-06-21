// scene that uses the SDF to decide how to bounce light.
#include "optics.h"


static bool DrawCircleAtNextPoint = false;

static void raytrace(Scene s, Vector2 start, Vector2 dir, Vector2 bottomLeft, Vector2 topRight) {
  const float MIN_TRACE_DIST = 1;
  dir = Vector2Normalize(dir);
  Vector2 pointCur = start;
  OpticMaterial matCur = materialQuery(s, start);

  const int NSTEPS = 30;
  static int maxSteps = 1;
  for(int isteps = 1; isteps <= NSTEPS; isteps++) {
    if (!inbounds(bottomLeft, pointCur, topRight)) { 
      maxSteps = std::max<int>(maxSteps, isteps);
      printf("stopped in max %5d steps\n", maxSteps);
      return;
    }

    // use distance to glass to decide length of ray.
    const float distToGlass = s.glassSDF->valueAt(pointCur);
    const float rayLength = std::max<float>(fabs(distToGlass) * 0.75, MIN_TRACE_DIST);
    Vector2 pointNext = Vector2Add(pointCur, Vector2Scale(dir, rayLength));
    OpticMaterial matNext = materialQuery(s, pointNext);

    // draw a circle showing how we shot the ray.
    if (DrawCircleAtNextPoint) { DrawCircle(pointNext.x, pointNext.y, 10, {100, 100, 100, 50}); }

    // refraction happened, we need to bend the direction now.
    if (matNext != matCur) {
      // change of medium.

      Vector2 normalOut = Vector2Normalize(s.glassSDF->dirOutwardAt(pointNext));
      // normal inward.
      Vector2 normalIn = Vector2Normalize(Vector2Negate(normalOut)); 
      const float cosIn = Vector2DotProduct(normalIn, dir);

      // decompose dir into dirProjNormalIn, dirRejNormalIn
      Vector2 dirProjNormalIn = Vector2Scale(normalIn, cosIn);
      Vector2 dirRejNormalIn = Vector2Subtract(dir, dirProjNormalIn);

      const float sinIn = sqrtf(1.0f - cosIn * cosIn);
      const float conservedIn = sinIn * matCur.refractiveIndex;
      const float sinOut = conservedIn / matNext.refractiveIndex;


      // only care if the angle is less than 90 degrees.
      if (true || cosIn > 0) {
        if (matNext.kind == OpticMaterialKind::Opaque) {
          //opaque, stop.
          DrawCircle(pointNext.x, pointNext.y, 3, BLACK);
          return;
        }
        else if (matNext.kind == OpticMaterialKind::Reflective) {
          // reflective.
          dir = Vector2Normalize(Vector2Add(dirRejNormalIn, Vector2Scale(dirProjNormalIn, -2)));

        } else if (matNext.kind == OpticMaterialKind::Refractive) {
          if (fabs(sinOut) >= 1) {
            // total internal reflection.
            dir = Vector2Normalize(Vector2Add(dirRejNormalIn, Vector2Scale(dirProjNormalIn, -2)));
            // DrawCircle(pointNext.x, pointNext.y, 2, (Color) {177, 175, 255, 255}); // total internal reflection color
          } else {
            // refraction..
            const float cosOut = sqrt(1 - sinOut * sinOut);
            Vector2 newDir = Vector2Add(Vector2Scale(dirProjNormalIn, cosOut), Vector2Scale(dirRejNormalIn, sinOut));
            dir = Vector2Normalize(newDir);
          } // end (sinOut > 1)
        } else {
          assert(false && "unknown MaterialKind.");
        }
      } // end (cosIn > 0)
    }
    Color c = { 120, 160, 131, 255}; // light ray color
    c.a = 255 * (1.0f - ((float)(isteps) / NSTEPS));
    DrawLineEx(pointCur, pointNext, 4, c);
    pointCur = pointNext;
    matCur = matNext;
  }
  return;
}


typedef struct {
  SDFCircle *circleLeft, *circleRight;
  SDFIntersect *lens;
  float lensRadius;
  float lensThickness;
  Vector2 lensCenter;
} sceneDData;

void* sceneD_init(void) {
    sceneDData *data = new sceneDData;
    data->lensRadius = 1000;
    data->lensThickness = 100;
    data->lensCenter = v2(0, 0);
    data->circleLeft = new SDFCircle();
    data->circleRight = new SDFCircle();
    data->lens = new SDFIntersect(data->circleLeft, data->circleRight);
    return data;
};


void sceneD_draw(void *raw_data) {
    sceneDData *data = (sceneDData*)raw_data;
    

    if (IsKeyPressed(KEY_SPACE)) {
      DrawCircleAtNextPoint = !DrawCircleAtNextPoint;
    }

    int midX = GetScreenWidth() / 2;
    int midY = GetScreenHeight() / 2;

    // update SDF
    data->lensThickness = std::max<int>(0, data->lensThickness + GetMouseWheelMove());
    data->circleLeft->radius = data->lensRadius;
    data->circleRight->radius = data->lensRadius;
    data->circleLeft->center.y = data->circleRight->center.y = midY;
    data->circleLeft->center.x = midX - data->lensRadius + data->lensThickness;
    data->circleRight->center.x = midX + data->lensRadius - data->lensThickness;

    BeginDrawing();
    ClearBackground({240, 240, 240, 255});
    Scene s; s.glassSDF = data->lens;

    const int NRAYS = 360;
    const Vector2 mousePos = GetMousePosition();
    for(float theta = 0; theta < M_PI * 2; theta += (M_PI * 2)/NRAYS) {
      std::pair<Vector2, bool> out;
      Vector2 raydir = v2(cos(theta), sin(theta));
      raytrace(s, mousePos, raydir, v2(0, 0), v2(GetScreenWidth(), GetScreenHeight()));
    }

    DrawFPS(10, 10);
    EndDrawing();
}
