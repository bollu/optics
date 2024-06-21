// scene that uses the SDF to decide how to bounce light.
#include "optics.h"


#define DISTANCE_APERTURE_TO_LENS 20

namespace SceneF {

static SDFResult sdfAABB(Vector2 topLeft, Vector2 bottomRight, Vector2 point) {
    assert(topLeft.x <= bottomRight.x);
    assert(topLeft.y <= bottomRight.y);
    // tl + (br - tl) * 0.5 = tl * 0.5 + br * 0.5 = (tl + br) * 0.5;
    Vector2 mid = Vector2Scale(Vector2Add(topLeft, bottomRight), 0.5);
    const float width = bottomRight.x - topLeft.x;
    const float height = bottomRight.y - topLeft.y;

    Vector2 delta = Vector2Subtract(point, mid);
    //                  ||||     [=======]
    // intersection of: |||| and [=======]
    //                  ||||     [=======]

    // 
    //                    mid + width/2
    // (p)---|----(mid)----|
    //       mid - width/2

    SDFResult resultX;
    if (point.x < mid.x - width * 0.5) {
      // outside.
      resultX.dist = (mid.x - width * 0.5) - point.x;
      resultX.dirOutward = v2(-1, 0);
    } else if (point.x > mid.x + width * 0.5) {
      // outside.
      resultX.dist = point.x - (mid.x - width * 0.5);
      resultX.dirOutward = v2(1, 0);
    } else {
      // inside.
      if (point.x < mid.x) {
        resultX.dist = (mid.x - width * 0.5) - point.x;
        resultX.dirOutward = v2(-1, 0);
      } else {
        resultX.dist = point.x - (mid.x + width * 0.5);
        resultX.dirOutward = v2(1, 0);
      }
    }


    SDFResult resultY;
    if (point.y < mid.y - height * 0.5) {
      // outside.
      resultY.dirOutward = v2(0, -1);
      resultY.dist = (mid.y - height * 0.5) - point.y;
    } else if (point.y > mid.y + height * 0.5) {
      // outside
      resultY.dirOutward = v2(0, 1);
      resultY.dist = point.y - (mid.y + height * 0.5);
    } else {
      // inside.
      if (point.y < mid.y) {
        resultY.dirOutward = v2(0, -1);
        resultY.dist = (mid.y - height * 0.5) - point.y;
      } else {
        resultY.dirOutward = v2(0, 1);
        resultY.dist = point.y - (mid.y + height * 0.5);
      }    
    }
    return resultX.dist > resultY.dist ? resultX : resultY;
}

struct ScreenData : public SDF {
  int x;
  int y;
  int halfWidth;
  int halfHeight;

  float valueAt(Vector2 point) {
     const Vector2 topLeft = v2(x - halfWidth, y - halfHeight);
    const Vector2 bottomRight = v2(x + halfWidth, y + halfHeight);
    return sdfAABB(topLeft, bottomRight, point).dist;
 
  }

  Vector2 dirOutwardAt(Vector2 point) {

    const Vector2 topLeft = v2(x - halfWidth, y - halfHeight);
    const Vector2 bottomRight = v2(x + halfWidth, y + halfHeight);
    return sdfAABB(topLeft, bottomRight, point).dirOutward;
  }

};


static void drawScreen(Scene s, ScreenData screenData) {
  Color color {128, 128, 128, 50};
    DrawLineEx(v2(screenData.x, screenData.y - screenData.halfHeight),
        v2(screenData.x, screenData.y + screenData.halfHeight),
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
  const float MIN_TRACE_DIST = 1;
  dir = Vector2Normalize(dir);
  Vector2 pointCur = start;
  RaytraceResult result;
  result.rayColor = rayColor;
  OpticMaterial matCur = materialQuery(s, start);

  const int NSTEPS = 1000;
  for(int isteps = 1; isteps <= NSTEPS; isteps++) {
    result.points.push_back(pointCur);
    if (!inbounds(bottomLeft, pointCur, topRight)) { 
      return result;
    }

    const float distToAperture = apertureData.valueAt(pointCur);
    if (distToAperture < 0) {
      result.intersectedAperture = true;
      return result; 
    }


    const float distToScreen = screenData.valueAt(pointCur);
    if (distToScreen < 0) {
      result.intersectedScreen = true;
      return result;
    }
    // use distance to glass to decide length of ray.
    const float distToGlass = s.glassSDF->valueAt(pointCur);
    float dist = 10000;
    dist = std::min<float>(dist, fabs(distToAperture));
    dist = std::min<float>(dist, fabs(distToGlass));
    dist = std::min<float>(dist, fabs(distToScreen));

    const float rayLength = std::max<float>(dist * 0.5, MIN_TRACE_DIST);
    Vector2 pointNext = Vector2Add(pointCur, Vector2Scale(dir, rayLength));
    OpticMaterial matNext = materialQuery(s, pointNext);

    // draw a circle showing how we shot the ray.
    // DrawCircle(pointNext.x, pointNext.y, 2, {100, 100, 100, 50});

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
          assert(false && "no opaque materials used.");
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

static Vector2 polarProject(Vector2 center, int radius, float theta) {
  return Vector2Add(center, v2(radius * cos(theta), radius * sin(theta)));
}

static void drawLens (sceneFData *data) {
  const Color borderColor = { 0, 0, 0, 50};
  const int NPOINTS = 1000;
  for(int i = 0; i < NPOINTS; ++i) {
    const float theta = M_PI * 2.0 * (float(i) / float(NPOINTS));
    const float thetaNext = M_PI * 2.0 * (float(i + 1) / float(NPOINTS));
    Vector2 ptCur = polarProject(data->circleLeft->center, data->circleLeft->radius, theta);
    Vector2 ptNext = polarProject(data->circleLeft->center, data->circleLeft->radius, thetaNext);

    if (data->circleRight->valueAt(ptCur) < 0 && data->circleRight->valueAt(ptNext) < 0) {
      DrawLineEx(ptCur, ptNext, 3, borderColor);
    }
  }
  for(int i = 0; i < NPOINTS; ++i) {
    const float theta = M_PI * 2.0 * (float(i) / float(NPOINTS));
    const float thetaNext = M_PI * 2.0 * (float(i + 1) / float(NPOINTS));
    Vector2 ptCur = polarProject(data->circleRight->center, data->circleRight->radius, theta);
    Vector2 ptNext = polarProject(data->circleRight->center, data->circleRight->radius, thetaNext);
    if (data->circleLeft->valueAt(ptCur) < 0 && data->circleLeft->valueAt(ptNext) < 0) {
      DrawLineEx(ptCur, ptNext, 3, borderColor);
    }
  }

}



};

using namespace SceneF;

void* sceneF_init(void) {
    sceneFData *data = new sceneFData;
    data->lensRadius = 10000;
    data->lensThickness = 10;
    data->lensCenter = v2(0, 0);
    data->circleLeft = new SDFCircle();
    data->circleRight = new SDFCircle();
    data->lens = new SDFIntersect(data->circleLeft, data->circleRight);
    data->apertureData.halfOpeningHeight = 0;
    data->apertureData.x = 0;
    return data;
};


float lensFocalLength(float lensRadius, float lensRefractiveIndex) {
  // https://physics.stackexchange.com/questions/168749/radius-of-curvature-and-focal-length
  // 1/f = (n - 1) (1/r1 + 1/r2)
  // 1/f = (n - 1) (2/r1)
  // f = r1 / 2(n - 1)
  return lensRadius / (2 * lensRefractiveIndex);
}

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
    data->circleLeft->center.x = midX * 1.2 - data->lensRadius + data->lensThickness;
    data->circleRight->center.x = midX * 1.2 + data->lensRadius - data->lensThickness;
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      data->lensThickness = std::max<int>(0, data->lensThickness + GetMouseWheelMove());
    } else {
      data->apertureData.halfOpeningHeight = std::max<int>(0, data->apertureData.halfOpeningHeight + 5 * GetMouseWheelMove());
    }
    data->apertureData.halfWidth = 10;
    data->apertureData.x = data->circleLeft->center.x + data->circleLeft->radius + 30 * data->lensThickness;

    data->screenData.x = data->apertureData.x + 20 * data->lensThickness;
    data->screenData.y = GetScreenHeight() / 2;
    data->screenData.halfWidth = 20;
    data->screenData.halfHeight = GetScreenHeight() / 4;

    BeginDrawing();
    ClearBackground({240, 240, 240, 255});

    DrawCircle((data->circleLeft->center.x + data->circleRight->center.x) * 0.5 - 
        lensFocalLength(data->circleLeft->radius, REFRACTIVE_INDEX_GLASS), data->circleLeft->center.y, 10, {255, 0, 0, 255});

    Scene s; s.glassSDF = data->lens;

    const int NPOINTS = 20;
    const int TOTAL_Y = 200;
    const Vector2 mousePos = GetMousePosition();
    for(int i = 0; i < NPOINTS; ++i) {
      float y = mousePos.y + (float(i - NPOINTS/2) / (NPOINTS/2)) * TOTAL_Y;
      Vector2 rayLoc = v2(mousePos.x, y);

      const unsigned char r = (float(i) / float(NPOINTS)) * 255;
      const unsigned char g = fabs(2 * (0.5 - float(i))) / float(NPOINTS) * 255;
      const unsigned char b = (1.0 - float(i) / float(NPOINTS)) * 255;;
      Color rayColor = {r, g, b, 255}; 

      const int NDIRS = 360;
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
          dotColor.a = 1;
          DrawCircle(x, y, 10, dotColor); 
        }
        if (result.intersectedScreen) {
          // if ((i + j) % 10 > 0) { continue; }
          Color lineColor = rayColor;
          lineColor.a = 5;
          drawLineSegmentSequence(result.points, 5, lineColor);
        } 
      }
    }

    drawAperture(s, data->apertureData);
    drawScreen(s, data->screenData);
    drawLens(data);

    DrawFPS(10, 10);
    EndDrawing();
}
