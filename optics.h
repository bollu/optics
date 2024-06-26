#pragma once
#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <assert.h>
#include <optional>
#include <functional>
#include "optics.h"

static const float TOLERANCE = 1e-3;

static Vector2 v2(float x, float y) {
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

struct SDFResult {
  Vector2 dirOutward;
  float dist;

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

struct SDFUnion : public SDF {
  SDF *s1, *s2;

  SDFUnion(SDF *s1, SDF *s2) : s1(s1), s2(s2) {}; 
  float valueAt(Vector2 point) {
    return std::min<float>(s1->valueAt(point), s2->valueAt(point));
  }
  Vector2 dirOutwardAt (Vector2 point) {
    if (s1->valueAt(point) < s2->valueAt(point)) {
      return s1->dirOutwardAt(point);
    } else {
      return s2->dirOutwardAt(point);
    }
  }
};

struct SDFAABB : public SDF {
  Vector2 topLeft = v2(0, 0);
  Vector2 bottomRight = v2(0, 0);
  SDFAABB() : topLeft(v2(0, 0)), bottomRight(v2(0, 0)) {}
  SDFAABB(Vector2 topLeft, Vector2 bottomRight) : topLeft(topLeft), bottomRight(bottomRight) {}


  Vector2 dirOutwardAt (Vector2 point) { return run(point).second; }

  float valueAt(Vector2 point) { return run(point).first; }

  std::pair<float, Vector2> run(Vector2 point) {
    assert(topLeft.x <= bottomRight.x);
    assert(topLeft.y <= bottomRight.y);
    // tl + (br - tl) * 0.5 = tl * 0.5 + br * 0.5 = (tl + br) * 0.5;
    Vector2 mid = Vector2Scale(Vector2Add(topLeft, bottomRight), 0.5);
    int width = bottomRight.x - topLeft.x;
    int height = bottomRight.y - topLeft.y;

    Vector2 delta = Vector2Subtract(point, mid);
    if (fabs(delta.x) < fabs(delta.y)) {
      return {fabs(delta.x) - width, v2(delta.x, 0)};
    } else {
      return {fabs(delta.y) - height, v2(0, delta.y)}; 
    }
  }
};

struct Scene {
  SDF *glassSDF;
};

static const float REFRACTIVE_INDEX_GLASS = 2;

static OpticMaterial materialQuery(Scene s, Vector2 point) {
  float dist = s.glassSDF->valueAt(point);
  if (dist > 0) {
    return OpticMaterial(OpticMaterialKind::Refractive, 1.0);
  } else {
    // glass
    return OpticMaterial(OpticMaterialKind::Refractive, REFRACTIVE_INDEX_GLASS);
  }
}

// cordinate system: top left (0, 0), positive x is right, positive y is bottom?
static bool inbounds(Vector2 bottomLeft, Vector2 cur, Vector2 topRight) {
  if (cur.x < bottomLeft.x) { return false; }
  if (cur.y < bottomLeft.y) { return false; }

  if (cur.x > topRight.x) { return false; }
  if (cur.y > topRight.y) { return false; }
  return true;
}


static float randFloat01() {
  return ((float)rand()/(float)(RAND_MAX));
}

