// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include <sceneRay/dag_sceneRay.h>
#include <sceneRay/dag_cachedRtVecFaces.h>
#include <math/dag_math3d.h>
#include <math/dag_bounds2.h>
#include <util/dag_bitArray.h>
#include <math/dag_capsuleTriangle.h>
#include <math/dag_capsuleTriangleCached.h>
#include <math/dag_rayIntersectSphere.h>
#include <math/dag_traceRayTriangle.h>
#include <math/dag_mathUtils.h>
#include <math/integer/dag_IPoint4.h>
#include <supp/dag_prefetch.h>

#include <debug/dag_log.h>
#include <debug/dag_debug.h>
#include <stdlib.h>
#include "version.h"

#include <EASTL/bitvector.h>
#include <memory/dag_framemem.h>

static inline unsigned get_u32(const SceneRayI24F8 &fi) { return fi.u32; }
static inline unsigned get_u32(const uint16_t &fi) { return StaticSceneRayTracer::CULL_CCW << 24; }

#if (__cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define IF_CONSTEXPR if constexpr
#else
#define IF_CONSTEXPR if
#endif

template <typename FI>
StaticSceneRayTracerT<FI>::StaticSceneRayTracerT() : skipFlags(USER_INVISIBLE << 24), useFlags(CULL_BOTH << 24), cullFlags(0)
{
  G_ASSERTF((((intptr_t)(this)) & 15) == 0, "this=%p", this);
  G_ASSERTF((((intptr_t)(&dump)) & 15) == 0, "this=%p dump=%p", this, &dump);
  G_ASSERTF((sizeof(dump) & 15) == 0, "sizeof(dump)=%d", sizeof(dump));
  G_ASSERTF((((intptr_t)(&v_rtBBox)) & 15) == 0, "this=%p v_rtBBox=%p", this, &v_rtBBox);
  v_bbox3_init_empty(v_rtBBox);
}

template <typename FI>
StaticSceneRayTracerT<FI>::~StaticSceneRayTracerT()
{}

template <typename FI>
void StaticSceneRayTracerT<FI>::Node::destroy()
{
  if (isNode())
  {
    getLeft()->destroy();
    getRight()->destroy();
    delete this;
  }
  else
  {
    delete ((LNode *)this);
  }
}

//--------------------------------------------------------------------------
template <typename FI>
inline int StaticSceneRayTracerT<FI>::tracerayLNode(const Point3 &p, const Point3 &dir, real &mint, const LNode *lnode,
  int ignore_face) const
{
  //--------------------------------------------------------------------------

  int hit = -1;
  const FaceIndex *__restrict fIndices = lnode->getFaceIndexStart();
  const FaceIndex *__restrict fIndicesEnd = lnode->getFaceIndexEnd();
  for (; fIndices < fIndicesEnd; fIndices++)
  {
    int fIndex = *fIndices; //(int)lnode->fc[fi];
    unsigned faceFlag = get_u32(*fIndices);
    IF_CONSTEXPR (sizeof(FaceIndex) > 2)
      if ((faceFlag & skipFlags) || !(faceFlag & useFlags))
        continue;
    if (fIndex == ignore_face)
      continue;

    // check culling
    faceFlag |= cullFlags;

    const RTface &f = faces(fIndex);

    const Point3_vec4 &vert0 = verts(f.v[0]);
    const Point3 edge1 = verts(f.v[1]) - vert0;
    const Point3 edge2 = verts(f.v[2]) - vert0;
    real u, v;
    if ((faceFlag & VAL_CULL_BOTH) == VAL_CULL_BOTH)
    {
      if (!traceRayToTriangleNoCull(p, dir, mint, vert0, edge1, edge2, u, v))
        continue;
    }
    else if (!traceRayToTriangleCullCCW(p, dir, mint, vert0, edge1, edge2, u, v))
      continue;
    hit = fIndex;
  }
  return hit;
}

//--------------------------------------------------------------------------
// Noinline because tracerayNodeVec called recursively

template <typename FI>
template <bool noCull>
DAGOR_NOINLINE int StaticSceneRayTracerT<FI>::tracerayLNodeVec(const vec3f &p, const vec3f &dir, float &t, const LNode *lnode,
  int ignore_face) const
{
  const FaceIndex *__restrict fIndices = lnode->getFaceIndexStart();
  const FaceIndex *__restrict fIndicesEnd = lnode->getFaceIndexEnd();
  int fIndex[4];
  int hit = -1;

  const uint32_t batchSize = 4;
  uint32_t count = 0;

  for (; fIndices < fIndicesEnd; fIndices++)
  {
    IF_CONSTEXPR (sizeof(FaceIndex) > 2)
      if (get_u32(*fIndices) & skipFlags || !(get_u32(*fIndices) & useFlags))
        continue;
    if (int(*fIndices) == ignore_face)
      continue;

    fIndex[count++] = int(*fIndices);
    if (count < batchSize)
      continue;
    count = 0;

    vec4f vert[batchSize][3];
    for (uint32_t i = 0; i < batchSize * 3; i++)
    {
      const RTface &f = faces(fIndex[i / 3]);
      vert[i / 3][i % 3] = v_ld(&verts(f.v[i % 3]).x);
    }

    int ret = traceray4Triangles(p, dir, t, vert, batchSize, noCull);
    if (ret >= 0)
      hit = fIndex[ret];
  }

  if (count)
  {
    alignas(EA_CACHE_LINE_SIZE) vec4f vert[batchSize][3];
    for (uint32_t i = 0; i < batchSize; i++)
    {
      if (i >= count) // unroll hint
        break;
      const RTface &f = faces(fIndex[i]);
      vert[i][0] = v_ld(&verts(f.v[0]).x);
      vert[i][1] = v_ld(&verts(f.v[1]).x);
      vert[i][2] = v_ld(&verts(f.v[2]).x);
    }

    int ret = traceray4Triangles(p, dir, t, vert, count, noCull);
    if (ret >= 0)
      hit = fIndex[ret];
  }

  return hit;
}

//--------------------------------------------------------------------------
template <typename FI>
template <bool noCull>
VECTORCALL inline int StaticSceneRayTracerT<FI>::rayhitLNodeIdx(vec3f p, vec3f dir, float t, const LNode *lnode, int ignore_face) const
{
  const FaceIndex *__restrict fIndices = lnode->getFaceIndexStart();
  const FaceIndex *__restrict fIndicesEnd = lnode->getFaceIndexEnd();
  IF_CONSTEXPR (sizeof(FaceIndex) > 2)
    for (; fIndices < fIndicesEnd; fIndices++)
    {
      unsigned faceFlag = get_u32(*fIndices);
      if ((faceFlag & skipFlags) || !(faceFlag & useFlags))
        continue;
      if (int(*fIndices) == ignore_face)
        continue;
      break;
    }
  if (fIndices >= fIndicesEnd)
    return -1;

#if !_TARGET_SIMD_SSE
  vec4f ret = v_zero();
#endif
  int fIndex0 = *fIndices;
  int fIndex[4] = {fIndex0, fIndex0, fIndex0, fIndex0};
  int fIndexCnt = 0;
  for (; fIndices < fIndicesEnd;)
  {
    fIndexCnt = 0;
    for (; fIndexCnt < 4 && fIndices < fIndicesEnd; fIndices++)
    {
      IF_CONSTEXPR (sizeof(FaceIndex) > 2)
      {
        unsigned faceFlag = get_u32(*fIndices);
        if ((faceFlag & skipFlags) || !(faceFlag & useFlags))
          continue;
      }
      fIndex[fIndexCnt] = *fIndices;
      fIndexCnt++;
    }

    const RTface *f[4] = {&faces(fIndex[0]), &faces(fIndex[1]), &faces(fIndex[2]), &faces(fIndex[3])};

    // vec3f vert0 = v_ld(&verts(f.v[0]).x);
    mat43f p0, p1, p2;
    v_mat44_transpose_to_mat43(v_ld(&verts(f[0]->v[0]).x), v_ld(&verts(f[1]->v[0]).x), v_ld(&verts(f[2]->v[0]).x),
      v_ld(&verts(f[3]->v[0]).x), p0.row0, p0.row1, p0.row2);
    v_mat44_transpose_to_mat43(v_ld(&verts(f[0]->v[1]).x), v_ld(&verts(f[1]->v[1]).x), v_ld(&verts(f[2]->v[1]).x),
      v_ld(&verts(f[3]->v[1]).x), p1.row0, p1.row1, p1.row2);
    v_mat44_transpose_to_mat43(v_ld(&verts(f[0]->v[2]).x), v_ld(&verts(f[1]->v[2]).x), v_ld(&verts(f[2]->v[2]).x),
      v_ld(&verts(f[3]->v[2]).x), p2.row0, p2.row1, p2.row2);

    if (int hit = rayhit4Triangles(p, dir, t, p0, p1, p2, noCull))
      return fIndex[__bsf_unsafe(hit)];
  }
  return -1;
}


//--------------------------------------------------------------------------
template <typename FI>
int StaticSceneRayTracerT<FI>::tracerayNode(const Point3 &p, const Point3 &dir, real &mint, const Node *node, int ignore_face) const
{
  if (!rayIntersectSphere(p, dir, node->bsc, node->bsr2, mint))
    return -1;
  if (node->isNode())
  {
    // branch node
    int hit = tracerayNode(p, dir, mint, node->getLeft(), ignore_face);
    int hit2;
    if ((hit2 = tracerayNode(p, dir, mint, node->getRight(), ignore_face)) != -1)
      return hit2;
    return hit;
  }
  else
  {
    // leaf node
    return tracerayLNode(p, dir, mint, (LNode *)node, ignore_face);
  }
}

template <typename FI>
template <bool noCull>
DAGOR_NOINLINE int StaticSceneRayTracerT<FI>::tracerayNodeVec(const vec3f &p, const vec3f &dir, float &t, const Node *node,
  int ignore_face) const
{
  vec4f bsc = v_ld(&node->bsc.x);
  if (!v_test_ray_sphere_intersection(p, dir, v_splats(t), bsc, v_splat_w(bsc)))
    return -1;
  if (node->isNode())
  {
    // branch node
    int hit0 = tracerayNodeVec<noCull>(p, dir, t, node->getLeft(), ignore_face);
    int hit1 = tracerayNodeVec<noCull>(p, dir, t, node->getRight(), ignore_face);
    return hit1 != -1 ? hit1 : hit0;
  }
  else
  {
    // leaf node
    return tracerayLNodeVec<noCull>(p, dir, t, (LNode *)node, ignore_face);
  }
}
//--------------------------------------------------------------------------

template <typename FI>
int StaticSceneRayTracerT<FI>::getHeightBelowLNode(const Point3 &p, real &ht, const LNode *lnode) const
{
  int ret = -1;
  const FaceIndex *__restrict fIndices = lnode->getFaceIndexStart();
  const FaceIndex *__restrict fIndicesEnd = lnode->getFaceIndexEnd();
  for (; fIndices < fIndicesEnd; fIndices++)
  {
    int fIndex = *fIndices; //(int)lnode->fc[fi];
    const RTface &f = faces(fIndex);

    const Point3 &vert0 = verts(f.v[0]);
    const Point3 edge1 = verts(f.v[1]) - vert0;
    const Point3 edge2 = verts(f.v[2]) - vert0;
    real triangleHt;
    if (!getTriangleHtCull<false>(p.x, p.z, triangleHt, vert0, edge1, edge2))
      continue;
    if (p.y >= triangleHt && ht <= triangleHt)
    {
      ret = fIndex;
      ht = triangleHt;
    }
  }
  return ret;
}

__forceinline static bool heightRayIntersectSphere(const Point3 &p, const Point3 &sphere_center, real r2, real cht)
{

  // reoder calculations for in-order cpus
  Point3 pc = sphere_center - p;
  real c = pc * pc - r2;
  if (c >= 0)
  {
    if (pc.y > 0)
      return false;

    real d = pc.y * pc.y - c; //*4.0f;
    if (d < 0)
      return false;

    real s = sqrtf(d);

    if (sphere_center.y - s > p.y)
      return false;
    if (sphere_center.y + s < cht)
      return false;
  }
  return true;
}

template <typename FI>
int StaticSceneRayTracerT<FI>::getHeightBelowNode(const Point3 &p, real &ht, const Node *node) const
{
  if (!heightRayIntersectSphere(p, node->bsc, node->bsr2, ht))
    return -1;
  if (node->isNode())
  {
    // branch node
    int hit = getHeightBelowNode(p, ht, node->getLeft());
    int hit2 = getHeightBelowNode(p, ht, node->getRight());
    if (hit2 != -1)
      return hit2;
    return hit;
  }
  else
  {
    // leaf node
    return getHeightBelowLNode(p, ht, (LNode *)node);
  }
}

//--------------------------------------------------------------------------
template <typename FI>
template <bool noCull>
VECTORCALL int StaticSceneRayTracerT<FI>::rayhitNodeIdx(vec3f p, vec3f dir, float t, const Node *node, int ignore_face) const
{
  vec4f bsc = v_ld(&node->bsc.x);
  if (!v_test_ray_sphere_intersection(p, dir, v_splats(t), bsc, v_splat_w(bsc)))
    return -1;
  if (node->isNode())
  {
    // branch node
    int idx = rayhitNodeIdx<noCull>(p, dir, t, node->getLeft(), ignore_face);
    if (idx >= 0)
      return idx;
    return rayhitNodeIdx<noCull>(p, dir, t, node->getRight(), ignore_face);
  }
  else
  {
    // leaf node
    return rayhitLNodeIdx<noCull>(p, dir, t, (LNode *)node, ignore_face);
  }
}

template <typename FI>
int StaticSceneRayTracerT<FI>::traceray(const Point3 &p, const Point3 &wdir2, real &mint2, int ignore_face) const
{
  Point3 wdir = wdir2;
  real dirl = lengthSq(wdir);
  if (dirl == 0)
  {
    return -1;
  }
  dirl = sqrtf(dirl);
  wdir /= dirl;
  real mint = mint2 * dirl;
  int id = tracerayNormalized(p, wdir, mint, ignore_face);
  if (id >= 0)
  {
    mint2 = mint / dirl;
  }
  return id;
}

template <typename FI>
VECTORCALL int StaticSceneRayTracerT<FI>::traceray(vec3f p, vec3f dir, real &mint, int ignore_face) const
{
  vec3f lenSq = v_length3_sq(dir);
  if (v_extract_x(lenSq) == 0.f)
    return -1;
  vec3f len = v_sqrt(lenSq);
  dir = v_div(dir, len);
  real t = mint * v_extract_x(len);
  int id = tracerayNormalized(p, dir, t, ignore_face);
  if (id >= 0)
    mint = t / v_extract_x(len);
  return id;
}

template <typename FI>
int StaticSceneRayTracerT<FI>::tracerayNormalized(const Point3 &p, const Point3 &dir, real &t, int ignore_face) const
{
  return tracerayNormalized(v_ldu(&p.x), v_ldu(&dir.x), t, ignore_face);
}

template <typename FI>
VECTORCALL int StaticSceneRayTracerT<FI>::tracerayNormalized(vec3f p, vec3f dir, real &mint, int ignore_face) const
{
  int ret = -1;
  if (mint <= 0)
    return -1;

  float shouldStartAt = 0.f;
  vec3f endPt = v_madd(dir, v_splats(mint), p);
  bbox3f bbox = v_ldu_bbox3(getBox());
  if (!v_bbox3_test_pt_inside(bbox, p))
  {
    vec4f startT = v_set_x(mint);
    if (!v_ray_box_intersection(p, dir, startT, bbox))
      return -1;
    shouldStartAt = v_extract_x(startT) * 0.9999f;
  }

  Point3_vec4 rayStart, rayDir;
  IPoint4 endCell;
  vec3f vEffectiveRayStart = v_madd(dir, v_splats(shouldStartAt), p);
  v_stu(&rayStart.x, vEffectiveRayStart);
  v_stu(&rayDir.x, dir);
  v_stui(&endCell.x, v_cvt_floori(v_div(endPt, v_ldu(&getLeafSize().x))));
  float effectiveT = mint - shouldStartAt;
  WooRay3d ray(rayStart, rayDir, getLeafSize());
  IPoint3 diff = IPoint3::xyz(endCell) - ray.currentCell();
  int n = 4 * (abs(diff.x) + abs(diff.y) + abs(diff.z)) + 1;
  double wooT = 0.f; // distance passed by WooRay
  for (; n; n--)
  {
    int leafSize = 0;
    if (getLeafLimits() & ray.currentCell())
    {
      auto getLeafRes = dump.grid->get_leaf(ray.currentCell().x, ray.currentCell().y, ray.currentCell().z);
      if (getLeafRes)
      {
        if (*getLeafRes.leaf)
        {
          // if ((cullFlags&VAL_CULL_BOTH) == VAL_CULL_BOTH)//that how it is supposed to be.
          int faceId = tracerayNodeVec<true>(vEffectiveRayStart, dir, effectiveT, *getLeafRes.leaf, ignore_face);
          // else
          //   faceId = tracerayNodeVec<false> (v_rayp, v_rayd, rmint, *getLeafRes.leaf, ignore_face);

          if (faceId != -1)
          {
            ret = faceId;
            if (effectiveT < wooT)
              goto end;
          }
        }
      }
      else
        leafSize = getLeafRes.getl() - 1;
    }
    IPoint3 oldpt = ray.currentCell();
    for (;;)
    {
      if (effectiveT < (wooT = ray.nextCell()))
        goto end;
      if (leafSize == 0 || (ray.currentCell().x >> leafSize) != (oldpt.x >> leafSize) ||
          (ray.currentCell().y >> leafSize) != (oldpt.y >> leafSize) || (ray.currentCell().z >> leafSize) != (oldpt.z >> leafSize))
        break;
    }
  }
end:
  if (ret >= 0)
    mint = shouldStartAt + effectiveT;
  return ret;
}

template <typename FI>
VECTORCALL bool StaticSceneRayTracerT<FI>::tracerayNormalized(vec3f p, vec3f dir, real mint, all_faces_ret_t &hits) const
{
  if (mint <= 0)
    return false;

  float shouldStartAt = 0.f;
  vec3f endPt = v_madd(dir, v_splats(mint), p);
  bbox3f bbox = v_ldu_bbox3(getBox());
  if (!v_bbox3_test_pt_inside(bbox, p))
  {
    vec4f startT = v_set_x(mint);
    if (!v_ray_box_intersection(p, dir, startT, bbox))
      return -1;
    shouldStartAt = v_extract_x(startT) * 0.9999f;
  }

  Point3_vec4 rayStart, rayDir;
  IPoint4 endCell;
  vec3f vEffectiveRayStart = v_madd(dir, v_splats(shouldStartAt), p);
  v_stu(&rayStart.x, vEffectiveRayStart);
  v_stu(&rayDir.x, dir);
  v_stui(&endCell.x, v_cvt_floori(v_div(endPt, v_ldu(&getLeafSize().x))));
  float effectiveT = mint - shouldStartAt;
  WooRay3d ray(rayStart, rayDir, getLeafSize());
  IPoint3 diff = IPoint3::xyz(endCell) - ray.currentCell();
  int n = 4 * (abs(diff.x) + abs(diff.y) + abs(diff.z)) + 1;
  double wooT = 0.f;
  for (; n; n--)
  {
    int leafSize = 0;
    if (getLeafLimits() & ray.currentCell())
    {
      auto getLeafRes = dump.grid->get_leaf(ray.currentCell().x, ray.currentCell().y, ray.currentCell().z);
      if (getLeafRes)
      {
        if (*getLeafRes.leaf)
        {
          float hitT = effectiveT;
          int faceId = tracerayNodeVec<true>(vEffectiveRayStart, dir, hitT, *getLeafRes.leaf, -1 /*ignore_face*/);
          if (faceId != -1)
            hits.emplace_back(FaceIntersection{faceId, shouldStartAt + hitT});
        }
      }
      else
        leafSize = getLeafRes.getl() - 1;
    }
    IPoint3 oldpt = ray.currentCell();
    for (;;)
    {
      if (effectiveT < (wooT = ray.nextCell()))
        return !hits.empty();
      if (leafSize == 0 || (ray.currentCell().x >> leafSize) != (oldpt.x >> leafSize) ||
          (ray.currentCell().y >> leafSize) != (oldpt.y >> leafSize) || (ray.currentCell().z >> leafSize) != (oldpt.z >> leafSize))
        break;
    }
  }
  return !hits.empty();
}

template <typename FI>
VECTORCALL int StaticSceneRayTracerT<FI>::rayhitNormalizedIdx(vec3f p, vec3f dir, real mint, int ignore_face) const
{
  if (mint <= 0)
    return -1;

  real shouldStartAt = 0.f;
  vec3f endPt = v_madd(dir, v_splats(mint), p);
  bbox3f bbox = v_ldu_bbox3(getBox());
  if (!v_bbox3_test_pt_inside(bbox, p))
  {
    vec4f startT = v_set_x(mint);
    if (!v_ray_box_intersection(p, dir, startT, bbox))
      return -1;
    shouldStartAt = v_extract_x(startT) * 0.9999f;
  }

  Point3_vec4 rayStart, rayDir;
  IPoint4 endCell;
  vec3f vEffectiveRayStart = v_madd(dir, v_splats(shouldStartAt), p);
  v_stu(&rayStart.x, vEffectiveRayStart);
  v_stu(&rayDir.x, dir);
  v_stui(&endCell.x, v_cvt_floori(v_div(endPt, v_ldu(&getLeafSize().x))));
  float effectiveT = mint - shouldStartAt;
  WooRay3d ray(rayStart, rayDir, getLeafSize());

  IPoint3 diff = IPoint3::xyz(endCell) - ray.currentCell();
  int n = 4 * (abs(diff.x) + abs(diff.y) + abs(diff.z)) + 1;
  double wooT = 0.f; // distance passed by WooRay
  for (; n; n--)
  {
    int leafSize = 0;
    IPoint3 oldpt = ray.currentCell();
    if (getLeafLimits() & ray.currentCell())
    {
      auto getLeafRes = dump.grid->get_leaf(oldpt.x, oldpt.y, oldpt.z);
      if (getLeafRes)
      {
        if (*getLeafRes.leaf)
        {
          int hitIdx = rayhitNodeIdx<true>(vEffectiveRayStart, dir, effectiveT, *getLeafRes.leaf, ignore_face);
          if (hitIdx >= 0)
            return hitIdx;
        }
      }
      else
        leafSize = getLeafRes.getl() - 1;
    }
    for (;;)
    {
      if (effectiveT < (wooT = ray.nextCell()))
        goto end;
      if (leafSize == 0 || (ray.currentCell().x >> leafSize) != (oldpt.x >> leafSize) ||
          (ray.currentCell().y >> leafSize) != (oldpt.y >> leafSize) || (ray.currentCell().z >> leafSize) != (oldpt.z >> leafSize))
        break;
    }
  }
end:
  return -1;
}

template <typename FI>
int StaticSceneRayTracerT<FI>::getHeightBelow(const Point3 &pos1, float &ht) const
{
  if (!(BBox2(Point2::xz(getBox()[0]), Point2::xz(getBox()[1])) & Point2::xz(pos1)))
    return -1;
  if (getBox()[0].y > pos1.y)
    return -1;
  if (getBox()[1].y < ht)
    return -1;
  Point3 p1 = pos1;
  p1.y = p1.y < getBox()[1].y ? p1.y : getBox()[1].y;
  float endY = getBox()[0].y < ht ? ht : getBox()[0].y;
  IPoint3 cell = ipoint3(floor(div(p1, getLeafSize())));
  cell.y = min(cell.y, getLeafLimits()[1].y);                                    // avoid nans leading to infinite cycle
  int endcellY = max((int)floorf(endY / getLeafSize().y), getLeafLimits()[0].y); // avoid nans leading to infinite cycle
  float cht = endY;
  int ret = -1, faceId;
  float maxCellHt = (cell.y + 1) * getLeafSize().y;
  for (; cell.y >= endcellY;)
  {
    auto getLeafRes = dump.grid->get_leaf(cell.x, cell.y, cell.z);
    if (getLeafRes)
    {
      if (*getLeafRes.leaf && (faceId = getHeightBelowNode(p1, cht, *getLeafRes.leaf)) >= 0)
        ret = faceId;
      maxCellHt -= getLeafSize().y;
      cell.y--;
    }
    else
    {
      int leafSize = getLeafRes.getl() - 1;
      cell.y &= ~((1 << leafSize) - 1);
      maxCellHt = cell.y * getLeafSize().y;
      cell.y--;
    }
    if (cht > maxCellHt)
      goto end;
  }
end:;
  if (ret >= 0)
  {
    ht = cht;
    return ret;
  }
  return -1;
}

template <typename FI>
int StaticSceneRayTracerT<FI>::traceDown(const Point3 &pos1, float &t) const
{
  float ht = pos1.y - t;
  int ret = getHeightBelow(pos1, ht);
  if (ret < 0)
    return -1;
  t = pos1.y - ht;
  return ret;
}


//--------------------------------------------------------------------------

template <typename FI>
inline bool StaticSceneRayTracerT<FI>::clipCapsuleFace(FaceIndex findex, const Capsule &c, Point3 &cp1, Point3 &cp2, real &md,
  const Point3 &movedir) const
{
  unsigned faceFlag = get_u32(findex);
  int fid = findex;
  IF_CONSTEXPR (sizeof(FaceIndex) > 2)
    if ((faceFlag & skipFlags) || !(faceFlag & useFlags))
      return false;

  const RTfaceBoundFlags &fb = facebounds(fid);
  if (!(faceFlag & VAL_CULL_CW) && fb.n * movedir > 0.001f)
    return false;
  else if (!(faceFlag & VAL_CULL_CCW) && fb.n * movedir < -0.001f)
    return false;

  Point3 cp1_f, cp2_f;
  real md_f = 0;
  const RTface &f = faces(fid);

  TriangleFace tf(verts(f.v[0]), verts(f.v[1]), verts(f.v[2]), fb.n);

  if (!clipCapsuleTriangle(c, cp1_f, cp2_f, md_f, tf))
    return false;

  /*if ( faceFlag & CULL_BOTH ) {
    tf.reverse();
    // check for collision with other side of face
    clipCapsuleTriangle (c, cp1_r, cp2_r, md_r, tf);

    // we take the closest value to 0 (i.e., min penetration depth)
    if ( md_r < 0 && md_r > md_f ) {
      // back side
      if ( md_r < md ) {
        md = md_r;
        cp1 = cp1_r;
        cp2 = cp2_r;
        return true;
      } else
        return false;
    }
  }*/

  if (md_f < md)
  {
    // front side
    md = md_f;
    cp1 = cp1_f;
    cp2 = cp2_f;
    return true;
  }
  return false;
}

//--------------------------------------------------------------------------
template <typename FI>
inline int StaticSceneRayTracerT<FI>::clipCapsuleLNode(const Capsule &c, Point3 &cp1, Point3 &cp2, real &md,
  const Point3 &movedirNormalized, const LNode *lnode) const
{
  //--------------------------------------------------------------------------
  int hit = -1;
  const FaceIndex *fIndices = lnode->getFaceIndexStart();
  const FaceIndex *fIndicesEnd = lnode->getFaceIndexEnd();
  for (; fIndices < fIndicesEnd; fIndices++)
  {
    int fIndex = *fIndices; //(int)lnode->fc[fi];
    if (clipCapsuleFace(*fIndices, c, cp1, cp2, md, movedirNormalized))
      hit = fIndex;
  }
  return hit;
}

//--------------------------------------------------------------------------
template <typename FI>
inline int StaticSceneRayTracerT<FI>::clipCapsuleNode(const Capsule &c, Point3 &cp1, Point3 &cp2, real &md,
  const Point3 &movedirNormalized, const Node *node) const
{
  //--------------------------------------------------------------------------
  // if (!capsuleIntersectsSphere(c, node->bsc, node->bsr2))
  //  return -1;
  if (node->isNode())
  {
    // branch node
    int hit = clipCapsuleNode(c, cp1, cp2, md, movedirNormalized, node->getLeft());
    int hit2;
    if ((hit2 = clipCapsuleNode(c, cp1, cp2, md, movedirNormalized, node->getRight())) != -1)
      return hit2;
    return hit;
  }
  else
  {
    return clipCapsuleLNode(c, cp1, cp2, md, movedirNormalized, (const LNode *)node);
  }
  return -1;
}

//! Tests for capsule clipping by scene; returns depth of penetration and normal to corresponding face
template <typename FI>
int StaticSceneRayTracerT<FI>::clipCapsule(const Capsule &c, Point3 &cp1, Point3 &cp2, real &md, const Point3 &movedirNormalized) const
{
  bbox3f cBox = c.getBoundingBox();
  if (!v_bbox3_test_box_intersect(cBox, v_ldu_bbox3(getBox())))
  {
    return -1;
  }
  // debug ( "clipCapsule start: md=%.3f", md );
  BBox3 sBox;
  v_stu_bbox3(sBox, cBox);

  int id = -1;
  IPoint3 b0(floor(div(sBox[0], getLeafSize())));
  IPoint3 b1(floor(div(sBox[1], getLeafSize())));
  getLeafLimits().clip(b0.x, b0.y, b0.z, b1.x, b1.y, b1.z);
  if (b1.x >= b0.x && b1.y >= b0.y && b1.z >= b0.z)
    for (int z = b0.z; z <= b1.z; ++z)
      for (int y = b0.y; y <= b1.y; ++y)
        for (int x = b0.x; x <= b1.x; ++x)
        {
          auto getLeafRes = dump.grid->get_leaf(x, y, z);
          if (!getLeafRes || !*getLeafRes.leaf)
            continue;

          int new_id = clipCapsuleNode(c, cp1, cp2, md, movedirNormalized, *getLeafRes.leaf);
          if (new_id != -1)
            id = new_id;
        }
  // debug ( "->id=%d, md=%.3f", id, md );
  return id;
}


template <typename FI>
template <typename U>
VECTORCALL __forceinline void StaticSceneRayTracerT<FI>::getFacesLNode(vec3f bmin, vec3f bmax, Tab<int> &face, U &uniqueness,
  const LNode *node, const StaticSceneRayTracerT *rt)
{
  const FaceIndex *fIndices = node->getFaceIndexStart();
  const FaceIndex *fIndicesEnd = node->getFaceIndexEnd();
  bbox3f box;
  box.bmin = bmin;
  box.bmax = bmax;
  for (; fIndices < fIndicesEnd; ++fIndices)
  {
    const int fidx = *fIndices;

    IF_CONSTEXPR (sizeof(FaceIndex) > 2)
    {
      unsigned faceFlag = get_u32(*fIndices);
      if ((faceFlag & rt->getSkipFlags()) || !(faceFlag & rt->getUseFlags()) || !(faceFlag & VAL_CULL_BOTH))
        continue;
    }

    if (uniqueness[fidx])
      continue;

    const RTface &f = rt->faces(fidx);
    bbox3f facebox;
    facebox.bmin = facebox.bmax = v_ld(&rt->verts(f.v[0]).x);
    v_bbox3_add_pt(facebox, v_ld(&rt->verts(f.v[1]).x));
    v_bbox3_add_pt(facebox, v_ld(&rt->verts(f.v[2]).x));

    if (!v_bbox3_test_box_intersect(box, facebox))
      continue;

    face.push_back(fidx);
    uniqueness.set(fidx, true);
  }
}

template <typename FI>
template <typename U>
VECTORCALL void StaticSceneRayTracerT<FI>::getFacesNode(vec3f bmin, vec3f bmax, Tab<int> &face, U &uniqueness, const Node *node,
  const StaticSceneRayTracerT *rt)
{
  if (node->isNode())
  {
    getFacesNode(bmin, bmax, face, uniqueness, node->getLeft(), rt);
    getFacesNode(bmin, bmax, face, uniqueness, node->getRight(), rt);
  }
  else
    getFacesLNode(bmin, bmax, face, uniqueness, (LNode *)node, rt);
}

template <typename FI>
int StaticSceneRayTracerT<FI>::getFaces(Tab<int> &face, const BBox3 &box) const
{
  const int startFc = face.size();

  IPoint3 b0(floor(::div(box.lim[0], dump.leafSize)));
  IPoint3 b1(floor(::div(box.lim[1], dump.leafSize)));

  getLeafLimits().clip(b0.x, b0.y, b0.z, b1.x, b1.y, b1.z);

  if (b1.x >= b0.x && b1.y >= b0.y && b1.z >= b0.z)
  {
    eastl::bitvector<framemem_allocator> uniqueness(getFacesCount());
    bbox3f bbox = v_ldu_bbox3(box);

    for (int z = b0.z; z <= b1.z; ++z)
      for (int y = b0.y; y <= b1.y; ++y)
        for (int x = b0.x; x <= b1.x; ++x)
        {
          auto getLeafRes = dump.grid->get_leaf(x, y, z);
          if (!getLeafRes || !*getLeafRes.leaf)
            continue;

          getFacesNode(bbox.bmin, bbox.bmax, face, uniqueness, *getLeafRes.leaf, this);
        }
  }

  return face.size() - startFc;
}

static StaticSceneRayTracerT<SceneRayI24F8>::GetFacesContext mtCtx32;
static StaticSceneRayTracerT<uint16_t>::GetFacesContext mtCtx16;

template <>
StaticSceneRayTracerT<SceneRayI24F8>::GetFacesContext *StaticSceneRayTracerT<SceneRayI24F8>::getMainThreadCtx() const
{
  return &mtCtx32;
}

template <>
StaticSceneRayTracerT<uint16_t>::GetFacesContext *StaticSceneRayTracerT<uint16_t>::getMainThreadCtx() const
{
  return &mtCtx16;
}

template <typename Node>
static void patch_node(Node &node, void *base, void *dump_base)
{
  G_ASSERTF((uintptr_t(&node) & 0xF) == 0, "node=%p", &node);
  if (node.isNode())
  {
    node.sub0.patch(base);
    node.sub1.patch(base);
    patch_node(*node.getRight(), base, dump_base);
    patch_node(*node.getLeft(), base, dump_base);
  }
  else
  {
    node.sub0.patch(dump_base);
    node.sub1.patch(dump_base);
  }
}

template <typename Node>
static void patch_legacy_node(Node &node, void *base, void *dump_base)
{
  if (node.sub0)
  {
    node.sub0.patch(base);
    node.sub1.patch(base);
    G_ASSERT(node.sub1.get() < node.sub0.get());
    patch_legacy_node(*node.getRight(), base, dump_base);
    patch_legacy_node(*node.getLeft(), base, dump_base);
  }
  else
  {
    struct LegacyLNode
    {
      Point3 bsc;
      real bsr2;
      PatchablePtr<Node> sub0;
      PatchablePtr<Node> faceStart;
      PatchablePtr<Node> faceEnd;
    };
    LegacyLNode *lnode = (LegacyLNode *)&node;
    lnode->faceStart.patch(dump_base);
    lnode->faceEnd.patch(dump_base);
    node.sub0 = lnode->faceStart;
    node.sub1 = lnode->faceEnd;
    G_ASSERT(node.sub0.get() < node.sub1.get());
  };
}

template <typename FI>
void StaticSceneRayTracerT<FI>::Dump::patch()
{
  vertsPtr.patch(this);
  facesPtr.patch(this);
  faceIndicesPtr.patch(this);
  grid.patch(this);
  if (version == LEGACY_VERSION)
    grid->patch(grid, this, [&](Node &node, void *base, void *dump_base) { patch_legacy_node(node, base, dump_base); });
  else
    grid->patch(grid, this, [&](Node &node, void *base, void *dump_base) { patch_node(node, base, dump_base); });
}

template <typename FI>
void StaticSceneRayTracerT<FI>::rearrangeLegacyDump(char *data, uint32_t n)
{
  auto *loadedDump = (Dump *)data;

  DynamicMemGeneralSaveCB gridSave(midmem, n);
  uintptr_t grid_ofs = uintptr_t(loadedDump->grid.get()) - uintptr_t(loadedDump);
  loadedDump->grid->serialize(gridSave, loadedDump);
  while ((grid_ofs & 15) != 8) // align grid on sizeof(vec4f)+8 to force grid::Node alignment to sizeof(vec4f)
    grid_ofs++;
  G_ASSERT(grid_ofs + gridSave.size() <= n);

  loadedDump->grid.setPtr(grid_ofs + (char *)loadedDump);
  memcpy(loadedDump->grid.get(), gridSave.data(), gridSave.size()); //-V780
  loadedDump->version = CURRENT_VERSION;
  loadedDump->grid->patch(loadedDump->grid, loadedDump,
    [&](Node &node, void *base, void *dump_base) { patch_node(node, base, dump_base); });
  dump = *loadedDump;
}

#include "serializableSceneRay.cpp"

template class StaticSceneRayTracerT<SceneRayI24F8>;
template class StaticSceneRayTracerT<uint16_t>;
