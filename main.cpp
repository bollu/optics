#include "raylib.h"
#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <assert.h>
#include <optional>
#include <functional>

static const float TOLERANCE = 1e-3;

Vector2 v2(float x, float y) {
    return Vector2{x, y};
}

enum class OpticMaterialKind {
  Reflective,
  Refractive,
  Opaque,
};

struct OpticMaterial {
  OpticMaterialKind kind;
  float refractiveIndex;

  OpticMaterial(OpticMaterialKind kind, float refractiveIndex) :
      kind(kind), refractiveIndex(refractiveIndex){};

  bool operator == (const OpticMaterial &other) const {
    return kind == other.kind && refractiveIndex == other.refractiveIndex;
  };

  bool operator != (const OpticMaterial &other) const {
    return !(*this == other);
  };
};

// signed distance function that also produces normal vectors.
struct SDF {
  virtual ~SDF() {};
  virtual float valueAt(Vector2 point) = 0;

  // return a potentially unnormalized vector in the normal outward direction.
  // This is the direction of the gradient.
  virtual Vector2 dirOutwardAt(Vector2 point) = 0;
};

struct SDFCircle : public SDF {

  Vector2 center;
  float radius;

  SDFCircle() : center(v2(0, 0)), radius(0) {}
  SDFCircle(Vector2 center, float radius) : center(center), radius(radius) {};

  float valueAt(Vector2 point) {
    return Vector2Length(Vector2Subtract(center, point)) - radius;
  };

  Vector2 dirOutwardAt(Vector2 point) {
    return Vector2Subtract(point, center);
  }
};

struct SDFIntersect : public SDF {
  SDF *s1, *s2;

  SDFIntersect(SDF *s1, SDF *s2) : s1(s1), s2(s2) {}; 
  float valueAt(Vector2 point) {
    return std::max<float>(s1->valueAt(point), s2->valueAt(point));
  }
  Vector2 dirOutwardAt (Vector2 point) {
    if (s1->valueAt(point) > s2->valueAt(point)) {
      return s1->dirOutwardAt(point);
    } else {
      return s2->dirOutwardAt(point);
    }
  }
};

struct Scene {
  SDF *glassSDF;
};

OpticMaterial materialQuery(Scene s, Vector2 point) {
  float dist = s.glassSDF->valueAt(point);
  if (dist > 0) {
    return OpticMaterial(OpticMaterialKind::Refractive, 1.0);
  } else {
    // glass
    // return OpticMaterial(OpticMaterialKind::Reflective, 1.0);
    return OpticMaterial(OpticMaterialKind::Refractive, 2.5);
  }
}

void raytrace(Scene s, Vector2 start, Vector2 dir) {
  const int NSTEPS = 3000;
  const float TRACE_DIST = 0.2;
  dir = Vector2Normalize(dir);
  Vector2 pointCur = start;
  OpticMaterial matCur = materialQuery(s, start);

  for(int i = 0; i < NSTEPS; ++i) {
    Vector2 pointNext = Vector2Add(pointCur, Vector2Scale(dir, TRACE_DIST));
    OpticMaterial matNext = materialQuery(s, pointNext);


    // refraction happened, we need to bend the direction now.
    if (matNext != matCur) {
      // change of medium.
      DrawCircle(pointNext.x, pointNext.y, 3, {100, 100, 100, 128});

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
            DrawCircle(pointNext.x, pointNext.y, 5, (Color) {226, 187, 223, 255}); // total internal reflection color
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
    Color c = { 90, 114, 160, 255}; // light ray color
    c.a = 255 * (1.0f - ((float)(i + 1) / NSTEPS));
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
} Scene1Data;

void* scene1_init(void) {
    Scene1Data *data = new Scene1Data;
    data->lensRadius = 1000;
    data->lensThickness = 10;
    data->lensCenter = v2(0, 0);
    data->circleLeft = new SDFCircle();
    data->circleRight = new SDFCircle();
    data->lens = new SDFIntersect(data->circleLeft, data->circleRight);
    return data;
};


void scene1_draw(Scene1Data *data) {
    
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

    const Vector2 mousePos = GetMousePosition();
    for(float theta = 0; theta < M_PI * 2; theta += (M_PI * 2)/60) {
      std::pair<Vector2, bool> out;
      Vector2 raydir = v2(cos(theta), sin(theta));
      raytrace(s, mousePos, raydir);
    }

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
