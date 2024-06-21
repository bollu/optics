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

typedef struct {
    Vector2 ballPosition;
    Vector2 ballSpeed;
    int ballRadius;
    bool pause;
    int framesCounter;
} Scene1Data;

void* scene1_init(void) {
    Scene1Data *data = new Scene1Data;
    data->ballPosition.x = GetScreenWidth()/2.0f;
    data->ballPosition.y = GetScreenHeight()/2.0f;
    data->ballSpeed.x = 5.0;
    data->ballSpeed.y = 4.0;
    data->ballRadius = 20;
    data->pause = 0;
    data->framesCounter = 0;
    return data;
};

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
      DrawCircle(pointNext.x, pointNext.y, 3, BLUE);
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
            DrawCircle(pointNext.x, pointNext.y, 5, (Color) {244, 143, 177, 255});
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
    Color c = { 144, 244, 210, 255};
    c.a = 255 * (1.0f - ((float)(i + 1) / NSTEPS));
    DrawLineEx(pointCur, pointNext, 4, c);
    pointCur = pointNext;
    matCur = matNext;
  }
  return;
}


#define REFRACTIVE_INDEX_GLASS 2 
#define REFRACTIVE_INDEX_AIR 1

void scene1_draw(Scene1Data *data) {
    
    int midX = GetScreenWidth() / 2;
    int midY = GetScreenHeight() / 2;

    BeginDrawing();
    ClearBackground(RAYWHITE);

    const Vector2 lensCenter = v2(midX, midY);
    Scene s;
    const int lensRadius = 3000;
    const int lensThickness = 10;
    s.glassSDF = new SDFIntersect(new SDFCircle(Vector2Add(lensCenter, v2(-lensRadius + lensThickness, 0)), lensRadius), 
        new SDFCircle(Vector2Add(lensCenter, v2(lensRadius - lensThickness, 0)), lensRadius));

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
