// scene that uses the SDF to decide how to bounce light.
#include "optics.h"


#define DISTANCE_APERTURE_TO_LENS 20

struct ApertureData : public SDF {
  int x;
  float halfOpeningHeight;
  float halfWidth;

  float valueAt(Vector2 point) {
    // distance from aperture.
    if (point.x < x - halfWidth) {
      return -(point.x - x - halfWidth);
    } else if (point.x > x + halfWidth) {
      return point.x - x - halfWidth;
    } else {
      // we are inside the box, get distance from the thickness.
      return halfOpeningHeight - fabs(point.y - GetScreenHeight() / 2);
    }
  }

  // return a potentially unnormalized vector in the normal outward direction.
  // This is the direction of the gradient.
  Vector2 dirOutwardAt(Vector2 point) {
    return point.x < x ? v2(-1, 0) : v2(1, 0);
  }
};


typedef struct {
  SDFCircle *circleLeft, *circleRight;
  SDFIntersect *lens;
  float lensRadius;
  float lensThickness;
  Vector2 lensCenter;
  ApertureData apertureData;
} sceneDData;


void drawAperture(Scene s, ApertureData apertureData) {
  Color color {0, 0, 0, 255};
    DrawLineEx(v2(apertureData.x, 0), 
        v2(apertureData.x, GetScreenHeight() / 2 - apertureData.halfOpeningHeight), 
        apertureData.halfWidth, color);

    DrawLineEx(v2(apertureData.x, GetScreenHeight() / 2 + apertureData.halfOpeningHeight),
        v2(apertureData.x, GetScreenHeight()),
        apertureData.halfWidth, color);

};

static bool DrawCircleAtNextPoint = false;


void drawPointSequence(const std::vector<Vector2> &points, int thickness, Color color) {
  for(int i = 0; i < points.size() - 1; ++i) {
    Vector2 cur = points[i];
    Vector2 next = points[i+1];
    DrawLineEx(cur, next, thickness, color);
  }
}

struct RaytraceResult {
  bool totalInternalReflected = false;
  bool refracted = false;
  bool intersectedAperture = false;
  std::vector<Vector2> points;

};

static RaytraceResult raytrace(Scene s, ApertureData &apertureData, Vector2 start, Vector2 dir, Vector2 bottomLeft, Vector2 topRight) {
  const float MIN_TRACE_DIST = 0.1;
  dir = Vector2Normalize(dir);
  Vector2 pointCur = start;
  RaytraceResult result;
  OpticMaterial matCur = materialQuery(s, start);

  const int NSTEPS = 300;
  static int maxSteps = 1;
  for(int isteps = 1; isteps <= NSTEPS; isteps++) {
    result.points.push_back(pointCur);
    if (!inbounds(bottomLeft, pointCur, topRight)) { 
      maxSteps = std::max<int>(maxSteps, isteps);
      printf("stopped in max %5d steps\n", maxSteps);
      return result;
    }


    const float distToAperture = apertureData.valueAt(pointCur);
    if (distToAperture < 0) {
      result.intersectedAperture = true;
      return result; 
    }
    // use distance to glass to decide length of ray.
    const float distToGlass = s.glassSDF->valueAt(pointCur);
    const float rayLength = std::max<float>(std::min<float>(fabs(distToAperture), fabs(distToGlass)) * 0.75, MIN_TRACE_DIST);
    Vector2 pointNext = Vector2Add(pointCur, Vector2Scale(dir, rayLength));
    OpticMaterial matNext = materialQuery(s, pointNext);

    // draw a circle showing how we shot the ray.
    // if (DrawCircleAtNextPoint) { DrawCircle(pointNext.x, pointNext.y, 10, {100, 100, 100, 50}); }

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
          return result;
        }
        else if (matNext.kind == OpticMaterialKind::Reflective) {
          // reflective.
          dir = Vector2Normalize(Vector2Add(dirRejNormalIn, Vector2Scale(dirProjNormalIn, -2)));

        } else if (matNext.kind == OpticMaterialKind::Refractive) {
          if (fabs(sinOut) >= 1) {
            // total internal reflection.
            result.totalInternalReflected = true;
            dir = Vector2Normalize(Vector2Add(dirRejNormalIn, Vector2Scale(dirProjNormalIn, -2)));
            // DrawCircle(pointNext.x, pointNext.y, 2, (Color) {177, 175, 255, 255}); // total internal reflection color
          } else {
            // refraction..
            result.refracted = true;
            const float cosOut = sqrt(1 - sinOut * sinOut);
            Vector2 newDir = Vector2Add(Vector2Scale(dirProjNormalIn, cosOut), Vector2Scale(dirRejNormalIn, sinOut));
            dir = Vector2Normalize(newDir);
          } // end (sinOut > 1)
        } else {
          assert(false && "unknown MaterialKind.");
        }
      } // end (cosIn > 0)
    }
    pointCur = pointNext;
    matCur = matNext;
  }
  return result;
}

void* sceneD_init(void) {
    sceneDData *data = new sceneDData;
    data->lensRadius = 10000;
    data->lensThickness = 100;
    data->lensCenter = v2(0, 0);
    data->circleLeft = new SDFCircle();
    data->circleRight = new SDFCircle();
    data->lens = new SDFIntersect(data->circleLeft, data->circleRight);
    data->apertureData.halfOpeningHeight = 0;
    data->apertureData.x = 0;
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
    // data->lensThickness = std::max<int>(0, data->lensThickness + GetMouseWheelMove());
    data->circleLeft->radius = data->lensRadius;
    data->circleRight->radius = data->lensRadius;
    data->circleLeft->center.y = data->circleRight->center.y = midY;
    data->circleLeft->center.x = midX - data->lensRadius + data->lensThickness;
    data->circleRight->center.x = midX + data->lensRadius - data->lensThickness;
    data->apertureData.halfOpeningHeight = std::max<int>(0, data->apertureData.halfOpeningHeight + 5 * GetMouseWheelMove());
    data->apertureData.halfWidth = 10;
    data->apertureData.x = data->circleRight->center.x - data->circleRight->radius - DISTANCE_APERTURE_TO_LENS - data->apertureData.halfWidth * 2;

    BeginDrawing();
    ClearBackground({240, 240, 240, 255});
    Scene s; s.glassSDF = data->lens;

    const int NPOINTS = 10;
    const int TOTAL_Y = 300;
    const Vector2 mousePos = GetMousePosition();
    for(int i = 0; i < NPOINTS; ++i) {
      float y = mousePos.y + (float(i - NPOINTS/2) / (NPOINTS/2)) * TOTAL_Y;
      Vector2 rayLoc = v2(mousePos.x, y);
      const int NDIRS = 1000;
      for (int j = 0; j <= NDIRS; ++j) {
        const float theta = (M_PI * 2.0) * ((float)j / (float)NDIRS);
        Vector2 rayDir = v2(cos(theta), sin(theta));
        RaytraceResult result = raytrace(s, data->apertureData, 
            rayLoc, rayDir, v2(0, 0), v2(GetScreenWidth(), GetScreenHeight()));
        if (result.refracted && !result.totalInternalReflected && !result.intersectedAperture) {
          const unsigned char r = (1.0 - float(i) / float(NPOINTS)) * 120;
          const unsigned char g = float(i) / float(NPOINTS) * 160;
          const unsigned char b = 255;
          Color c = { r, g, b, 20}; 
          drawPointSequence(result.points, 3, c);
        } else if (result.intersectedAperture) {
          Color c = { 200, 200, 200, 5};
          drawPointSequence(result.points, 4, c);
        }
      }
    }

    drawAperture(s, data->apertureData);

    DrawFPS(10, 10);
    EndDrawing();
}
