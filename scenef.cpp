// scene that uses the SDF to decide how to bounce light.
#include "optics.h"


#define DISTANCE_APERTURE_TO_LENS 20


static SDFResult sdfAABB(Vector2 topLeft, Vector2 bottomRight, Vector2 point) {
    assert(topLeft.x <= bottomRight.x);
    assert(topLeft.y <= bottomRight.y);
    // tl + (br - tl) * 0.5 = tl * 0.5 + br * 0.5 = (tl + br) * 0.5;
    Vector2 mid = Vector2Scale(Vector2Add(topLeft, bottomRight), 0.5);
    const int width = bottomRight.x - topLeft.x;
    const int height = bottomRight.y - topLeft.y;

    Vector2 delta = Vector2Subtract(point, mid);
    SDFResult result;
    //                  ||||     [=======]
    // intersection of: |||| and [=======]
    //                  ||||     [=======]
    if (fabs(delta.x) > fabs(delta.y)) {
      result.dist = fabs(delta.x) - width * 0.5;
      result.dirOutward = v2(delta.x, 0);
    } else {
      result.dist = fabs(delta.y) - height * 0.5;
      result.dirOutward = v2(0, delta.y);
    }
    return result;
}

struct ScreenData : public SDF {
  int x;
  int halfWidth;
  int halfHeight;

  float valueAt(Vector2 point) {
    const Vector2 topLeft = v2(x - halfWidth, GetScreenHeight() / 2 - halfHeight);
    const Vector2 bottomRight = v2(x + halfWidth, GetScreenHeight() / 2 + halfHeight);
    return sdfAABB(topLeft, bottomRight, point).dist;
  }

  Vector2 dirOutwardAt(Vector2 point) {
    const Vector2 topLeft = v2(x - halfWidth, GetScreenHeight() / 2 - halfHeight);
    const Vector2 bottomRight = v2(x + halfWidth, GetScreenHeight() / 2 + halfHeight);
    return sdfAABB(topLeft, bottomRight, point).dirOutward;
  }

};


static void drawScreen(Scene s, ScreenData screenData) {
  Color color {128, 128, 128, 50};
    DrawLineEx(v2(screenData.x, GetScreenHeight() / 2 - screenData.halfHeight),
        v2(screenData.x, GetScreenHeight() / 2 + screenData.halfHeight),
        screenData.halfWidth * 2, color);
}

struct ApertureData : public SDF {
  int x = 0;
  float halfOpeningHeight = 0;
  float halfWidth = 0;

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
  ScreenData screenData;
} sceneFData;


static void drawAperture(Scene s, ApertureData apertureData) {
  Color color {160, 147, 125, 255};
    DrawLineEx(v2(apertureData.x, 0), 
        v2(apertureData.x, GetScreenHeight() / 2 - apertureData.halfOpeningHeight), 
        apertureData.halfWidth, color);

    DrawLineEx(v2(apertureData.x, GetScreenHeight() / 2 + apertureData.halfOpeningHeight),
        v2(apertureData.x, GetScreenHeight()),
        apertureData.halfWidth, color);

};

static bool DrawCircleAtNextPoint = false;


static void drawLineSegmentSequence(const std::vector<Vector2> &points, int thickness, Color color) {
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
  Color rayColor;
  bool intersectedScreen = false;
  std::vector<Vector2> points = {};
};

static RaytraceResult raytrace(Scene s, 
    Color rayColor,
    ApertureData &apertureData, 
    ScreenData &screenData,
    Vector2 start, Vector2 dir, Vector2 bottomLeft, Vector2 topRight) {
  const float MIN_TRACE_DIST = 0.01;
  dir = Vector2Normalize(dir);
  Vector2 pointCur = start;
  RaytraceResult result;
  result.rayColor = rayColor;
  OpticMaterial matCur = materialQuery(s, start);

  const int NSTEPS = 100;
  for(int isteps = 1; isteps <= NSTEPS; isteps++) {
    result.points.push_back(pointCur);
    if (!inbounds(bottomLeft, pointCur, topRight)) { 
      return result;
    }


    const float distToScreen = screenData.valueAt(pointCur);
    if (distToScreen < 0) {
      result.intersectedScreen = true;
      return result;
    }
    const float distToAperture = apertureData.valueAt(pointCur);
    if (distToAperture < 0) {
      result.intersectedAperture = true;
      return result; 
    }
    // use distance to glass to decide length of ray.
    const float distToGlass = s.glassSDF->valueAt(pointCur);
    float dist = 10000;
    dist = std::min<float>(dist, fabs(distToAperture));
    dist = std::min<float>(dist, fabs(distToGlass));
    dist = std::min<float>(dist, fabs(distToScreen));

    const float rayLength = std::max<float>(dist * 0.9, MIN_TRACE_DIST);
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

void* sceneF_init(void) {
    sceneFData *data = new sceneFData;
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

void sceneF_draw(void *raw_data) {
    sceneFData *data = (sceneFData*)raw_data;
    

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
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      data->lensThickness = std::max<int>(0, data->lensThickness + GetMouseWheelMove());
    } else {
      data->apertureData.halfOpeningHeight = std::max<int>(0, data->apertureData.halfOpeningHeight + 5 * GetMouseWheelMove());
    }
    data->apertureData.halfWidth = 10;
    data->apertureData.x = midX + 3 * data->lensThickness;

    data->screenData.x = data->apertureData.x + 3 * data->lensThickness;
    data->screenData.halfWidth = 20;
    data->screenData.halfHeight = GetScreenHeight() / 4;

    BeginDrawing();
    ClearBackground({240, 240, 240, 255});
    Scene s; s.glassSDF = data->lens;

    const int NPOINTS = 10;
    const int TOTAL_Y = 150;
    const Vector2 mousePos = GetMousePosition();
    for(int i = 0; i < NPOINTS; ++i) {
      float y = mousePos.y + (float(i - NPOINTS/2) / (NPOINTS/2)) * TOTAL_Y;
      Vector2 rayLoc = v2(mousePos.x, y);

      const unsigned char r = (float(i) / float(NPOINTS)) * 255;
      const unsigned char g = fabs(2 * (0.5 - float(i))) / float(NPOINTS) * 255;
      const unsigned char b = (1.0 - float(i) / float(NPOINTS)) * 255;;
      Color rayColor = {r, g, b, 20}; 

      const int NDIRS = 600;
      for (int j = 0; j <= NDIRS; ++j) {
        const float theta = (M_PI * 2.0) * ((float)j / (float)NDIRS);
        Vector2 rayDir = v2(cos(theta), sin(theta));
        RaytraceResult result = raytrace(s, rayColor, 
            data->apertureData,  data->screenData,
            rayLoc, rayDir, v2(0, 0), v2(GetScreenWidth(), GetScreenHeight()));
        if (result.intersectedScreen && result.points.size() > 0) {
          const float y = result.points[result.points.size() - 1].y;
          const float x = data->screenData.x;
          Color dotColor = rayColor;
          dotColor.a = 100;
          DrawCircle(x, y, 3, dotColor); 
          // DrawLineEx(cur, next, data->screenData.halfWidth / 4, color);
        }
        if (result.intersectedScreen) {
          drawLineSegmentSequence(result.points, 3, rayColor);
        } 
      }
    }

    drawAperture(s, data->apertureData);
    drawScreen(s, data->screenData);

    DrawFPS(10, 10);
    EndDrawing();
}
