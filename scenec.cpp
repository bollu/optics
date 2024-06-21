// scene where light rays are importance sampled, slowly.
#include "optics.h"

struct RaytraceResults {
  int nreflections = 0;
  int nrefractions = 0;
  int nsteps = 0;

  float getImportance() {
    return  nsteps;

  }
};

RaytraceResults raytrace(Scene s, Vector2 start, Vector2 dir, Vector2 bottomLeft, Vector2 topRight) {
  RaytraceResults results;
  const float MIN_TRACE_DIST = 1;
  dir = Vector2Normalize(dir);
  Vector2 pointCur = start;
  OpticMaterial matCur = materialQuery(s, start);

  const int NSTEPS = 100;
  static int maxSteps = 1;
  for(int isteps = 1; isteps <= NSTEPS; isteps++) {
    results.nsteps++;
    if (!inbounds(bottomLeft, pointCur, topRight)) { 
      return results;
    }

    // use distance to glass to decide length of ray.
    const float distToGlass = s.glassSDF->valueAt(pointCur);
    const float rayLength = std::max<float>(MIN_TRACE_DIST, 0.8 * distToGlass);
    Vector2 pointNext = Vector2Add(pointCur, Vector2Scale(dir, rayLength));
    OpticMaterial matNext = materialQuery(s, pointNext);

    // draw a circle showing how we shot the ray.
    // DrawCircle(pointNext.x, pointNext.y, 3, {100, 100, 100, 50});

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
          return results;
        }
        else if (matNext.kind == OpticMaterialKind::Reflective) {
          results.nreflections++;
          // reflective.
          dir = Vector2Normalize(Vector2Add(dirRejNormalIn, Vector2Scale(dirProjNormalIn, -2)));

        } else if (matNext.kind == OpticMaterialKind::Refractive) {
          results.nrefractions++;
          if (fabs(sinOut) >= 1) {
            // total internal reflection.
            dir = Vector2Normalize(Vector2Add(dirRejNormalIn, Vector2Scale(dirProjNormalIn, -2)));
            // DrawCircle(pointNext.x, pointNext.y, c, (Color) {177, 175, 255, 255}); // total internal reflection color
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
    Color c = { 255, 255, 255, 1}; // light ray color
    // c.a = 10 * (1.0f - ((float)(isteps) / NSTEPS));
    DrawLineEx(pointCur, pointNext, 4, c);
    pointCur = pointNext;
    matCur = matNext;
  }
  return results;
}


typedef struct {
  SDFCircle *circleLeft, *circleRight;
  SDFIntersect *lens;
  float lensRadius;
  float lensThickness;
  Vector2 lensCenter;
  Vector2 mousePos;
  std::vector<float> thetas;
} sceneCData;

void* sceneC_init(void) {
    sceneCData *data = new sceneCData;
    data->lensRadius = 1000;
    data->lensThickness = 100;
    data->lensCenter = v2(0, 0);
    data->circleLeft = new SDFCircle();
    data->circleRight = new SDFCircle();
    data->mousePos = v2(0, 0);
    data->lens = new SDFIntersect(data->circleLeft, data->circleRight);
    return data;
};

void sceneC_draw(void *raw_data) {
    sceneCData *data = (sceneCData *)raw_data;
    
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
    ClearBackground({0, 0, 0, 255});
    Scene s; s.glassSDF = data->lens;

    // const int NRAYS = 180;
    const Vector2 curMousePos = GetMousePosition();
    if (curMousePos.x != data->mousePos.x || curMousePos.y != data->mousePos.y) {
      data->thetas.clear();
    }
    data->mousePos = curMousePos;
    static float curImportance = 1e-3;
    static float curTheta = 0;


    const int NSAMPLES_PER_FRAME = 50;
    for(int i = 0; i < NSAMPLES_PER_FRAME; ++i) {
      const float nextTheta = curTheta + (randFloat01() > 0.5 ? 1 : -1 ) * randFloat01() * M_PI / 10;
      data->thetas.push_back(nextTheta);
      Vector2 raydir = v2(cos(nextTheta), sin(nextTheta));
      RaytraceResults result = raytrace(s, curMousePos, raydir, v2(0, 0), v2(GetScreenWidth(), GetScreenHeight()));
      const float nextImportance = result.getImportance();
      // metropolois hastings
      if (randFloat01() < nextImportance / curImportance) {
        curTheta = nextTheta;
        curImportance = nextImportance;
      } 
    }

    for(int i = 0; i < data->thetas.size() - 1; ++i) {
      float theta = data->thetas[i];
      Vector2 raydir = v2(cos(theta), sin(theta));
      raytrace(s, curMousePos, raydir, v2(0, 0), v2(GetScreenWidth(), GetScreenHeight()));
    }

    DrawFPS(10, 10);
    EndDrawing();
}
