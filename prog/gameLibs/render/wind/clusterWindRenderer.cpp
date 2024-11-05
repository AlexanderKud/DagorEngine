// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include <render/wind/clusterWindRenderer.h>
#include <render/wind/clusterWindCascade.h>

#include <render/wind/clusterWind.h>

#include <3d/dag_sbufferIDHolder.h>
#include <drv/3d/dag_driver.h>
#include <shaders/dag_shaders.h>
#include <memory/dag_framemem.h>

#include <math/dag_mathAng.h>
#include <3d/dag_resPtr.h>

#include <perfMon/dag_statDrv.h>

// GPU side
static SbufferIDHolderWithVar cluster_buf;
static SbufferIDHolderWithVar cluster_buf_prev;
static int treeBendingMultVarId = -1;
static int impostorBendingMultVarId = -1;
static int grassBendingMultVarId = -1;
static int treeBendingAnimationMultVarId = -1;
static int grassBendingAnimationMultVarId = -1;
static int cascadeNumVarId = -1;

static unsigned int pack_value(float value, float precision, float min, float max)
{
  int endValue = roundf((value - min) / (max - min) * precision);
  return endValue;
}

void ClusterWindRenderer::updateClusterBuffer(const void *clusters, int num)
{
  if (num <= 0)
    return;
  clusterDescArr[currentClusterIndex].resize(num);
  Tab<ClusterWind::ClusterDesc> tempArr(framemem_ptr());
  tempArr.resize(num);
  memcpy(tempArr.data(), clusters, num * sizeof(ClusterWind::ClusterDesc));
  for (int i = 0; i < tempArr.size(); ++i)
  {
    ClusterDescGpu &clusterDesc = clusterDescArr[currentClusterIndex][i];
    float precision = BYTE_2;
    float maxHalfPrecision = (precision - 1.0) * 0.5;
    float minHalfPrecision = -maxHalfPrecision;

    clusterDesc.time = (unsigned int)pack_value(tempArr[i].time / tempArr[i].maxTime, BYTE_2, 0.0f, 1.0f) << SHIFT_2_BYTES;

    clusterDesc.sphere.x = (unsigned int)pack_value(tempArr[i].sphere.x, precision, minHalfPrecision, maxHalfPrecision)
                           << SHIFT_2_BYTES;
    clusterDesc.sphere.x += (unsigned int)pack_value(tempArr[i].sphere.y, precision, minHalfPrecision, maxHalfPrecision);
    clusterDesc.sphere.y = (unsigned int)pack_value(tempArr[i].sphere.z, precision, minHalfPrecision, maxHalfPrecision)
                           << SHIFT_2_BYTES;
    clusterDesc.sphere.y += (unsigned int)pack_value(tempArr[i].sphere.w, precision, minHalfPrecision, maxHalfPrecision);
    clusterDesc.power = (unsigned int)pack_value(tempArr[i].power, precision, 0.0f, MAX_POWER_FOR_BLAST);
  }
}

void ClusterWindRenderer::updateClusterWindGridsBuffer(const Tab<ClusterWindCascade> &clustersGrids)
{
  int numOfBox = 0;
  for (int i = 0; i < clustersGrids.size(); ++i)
    numOfBox += sqr(clustersGrids[i].boxWidthNum);
  gridsIdArr[currentClusterIndex].resize(numOfBox / 4);
  for (int i = 0; i < clustersGrids.size(); ++i)
    for (int j = 0; j < sqr(clustersGrids[i].boxWidthNum); ++j)
      gridsIdArr[currentClusterIndex][(j + i * sqr(clustersGrids[max(0, i - 1)].boxWidthNum)) / 4]
                [(j + i * sqr(clustersGrids[max(0, i - 1)].boxWidthNum)) % 4] = clustersGrids[i].getAllClusterId(j);
}

void ClusterWindRenderer::updateClusterWindGridsDesc(const Tab<ClusterWindCascade> &clustersGrids)
{
  cascadeDescArr[currentClusterIndex].resize(clustersGrids.size());
  for (int i = 0; i < clustersGrids.size(); ++i)
  {
    float precision = BYTE_2;
    float maxHalfPrecision = (precision - 1.0) * 0.5;
    float minHalfPrecision = -maxHalfPrecision;
    ClusterCascadeDescGpu &clusterCascadeDesc = cascadeDescArr[currentClusterIndex][i];
    const ClusterWindCascade &clusterWindCascade = clustersGrids[i];
    clusterCascadeDesc.pos =
      (unsigned int)pack_value(clusterWindCascade.getActualPosition().x, precision, minHalfPrecision, maxHalfPrecision)
      << SHIFT_2_BYTES;
    clusterCascadeDesc.pos +=
      (unsigned int)pack_value(clusterWindCascade.getActualPosition().z, precision, minHalfPrecision, maxHalfPrecision);
    clusterCascadeDesc.gridSize = clusterWindCascade.size;
    clusterCascadeDesc.boxWidthNum = clusterWindCascade.boxWidthNum;
    clusterCascadeDesc.clusterPerBox = clusterWindCascade.maxClusterPerBox;
  }

  ShaderGlobal::set_int(cascadeNumVarId, clustersGrids.size());
}

void ClusterWindRenderer::updateSecondClusterWindGridsDesc()
{
  cascadeDescArr[(currentClusterIndex + 1u) % 2].resize(cascadeDescArr[currentClusterIndex].size());
  for (int i = 0; i < cascadeDescArr[currentClusterIndex].size(); ++i)
  {
    cascadeDescArr[(currentClusterIndex + 1u) % 2][i] = cascadeDescArr[currentClusterIndex][i];
  }
}

void ClusterWindRenderer::updateRenderer()
{
  TIME_PROFILE(ClusterWindUpdateGPU);

  if (cluster_buf_prev.getBuf())
    cluster_buf.getBuf()->copyTo(cluster_buf_prev.getBuf());

  uint8_t *data = nullptr;
  if (!cluster_buf.getBuf()->lock(0, 0, (void **)&data, VBLOCK_WRITEONLY | VBLOCK_DISCARD) || !data)
  {
    static bool warned = false;
    if (!warned)
    {
      logwarn("clusterWind Buffer fail to lock");
      warned = true;
    }
    return;
  }
  const size_t cascadeDataSize = MAXIMUM_GRID_BOX_CASCADE * sizeof(uint4);
  const size_t gridsDataSize = ((MAX_BOX_PER_GRID * MAXIMUM_GRID_BOX_CASCADE) / 4) * sizeof(uint4);
  const size_t maxSize = cascadeDescArr[currentClusterIndex].size() * sizeof(uint4) +
                         gridsIdArr[currentClusterIndex].size() * sizeof(uint4) +
                         clusterDescArr[currentClusterIndex].size() * sizeof(uint4);
  if (maxSize > (cluster_buf.getBuf()->getNumElements() * cluster_buf.getBuf()->getElementSize()))
  {
    static bool warned = false;
    if (!warned)
    {
      logwarn("cascadeDescArr count : %d, gridsIdArr count = %d, clusterDescArr count = %d",
        cascadeDescArr[currentClusterIndex].size(), gridsIdArr[currentClusterIndex].size(),
        clusterDescArr[currentClusterIndex].size());
    }
    G_ASSERT_RETURN(false, );
  }
  memcpy(data, cascadeDescArr[currentClusterIndex].data(), cascadeDescArr[currentClusterIndex].size() * sizeof(uint4));
  memcpy(data + cascadeDataSize, gridsIdArr[currentClusterIndex].data(), gridsIdArr[currentClusterIndex].size() * sizeof(uint4));
  memcpy(data + cascadeDataSize + gridsDataSize, clusterDescArr[currentClusterIndex].data(),
    clusterDescArr[currentClusterIndex].size() * sizeof(uint4));
  cluster_buf.getBuf()->unlock();
  cluster_buf.setVar();
  cluster_buf_prev.setVar();
}

void ClusterWindRenderer::loadBendingMultConst(float treeMult, float impostorMult, float grassMult, float treeAnimationMult,
  float grassAnimationMult)
{
  ShaderGlobal::set_real(treeBendingMultVarId, treeMult);
  ShaderGlobal::set_real(impostorBendingMultVarId, impostorMult);
  ShaderGlobal::set_real(grassBendingMultVarId, grassMult);

  ShaderGlobal::set_real(treeBendingAnimationMultVarId, treeAnimationMult);
  ShaderGlobal::set_real(grassBendingAnimationMultVarId, grassAnimationMult);
}

void ClusterWindRenderer::updateCurrentRendererIndex()
{
  updateSecondClusterWindGridsDesc(); // update on thread safe space
  currentClusterIndex = ((currentClusterIndex + 1u) % 2u);
}

bool ClusterWindRenderer::getClusterDescForCpuSim(int num, ClusterDescGpu &outputClusterDescGpu)
{
  if (num >= clusterDescArr[(currentClusterIndex + 1u) % 2u].size())
    return false;
  outputClusterDescGpu = clusterDescArr[(currentClusterIndex + 1u) % 2u][num];
  return true;
}

bool ClusterWindRenderer::getClusterGridsIdForCpuSim(int num, uint4 &outGridsId)
{
  if (num >= gridsIdArr[(currentClusterIndex + 1u) % 2u].size() * 4u)
    return false;
  outGridsId = gridsIdArr[(currentClusterIndex + 1u) % 2u][num / 4];
  return true;
}

ClusterWindRenderer::ClusterWindRenderer(bool need_historical_buffer)
{
  int bufferSize = (MAXIMUM_GRID_BOX_CASCADE + (MAX_BOX_PER_GRID * MAXIMUM_GRID_BOX_CASCADE) / 4 + MAX_CLUSTER);

  cluster_buf.set(d3d::buffers::create_persistent_cb(bufferSize, "cluster_buf"), "cluster_buf");

  if (need_historical_buffer)
    cluster_buf_prev.set(d3d::buffers::create_persistent_cb(bufferSize, "cluster_buf_prev"), "cluster_buf_prev");

  treeBendingMultVarId = get_shader_variable_id("cluster_wind_tree_bending_mult");
  impostorBendingMultVarId = get_shader_variable_id("cluster_wind_impostor_bending_mult");
  grassBendingMultVarId = get_shader_variable_id("cluster_wind_grass_bending_mult");

  treeBendingAnimationMultVarId = get_shader_variable_id("cluster_wind_tree_animation_mult");
  grassBendingAnimationMultVarId = get_shader_variable_id("cluster_wind_grass_animation_mult");

  cascadeNumVarId = get_shader_variable_id("cluster_wind_cascade_num");

  currentClusterIndex = 0;
  for (auto &arr : clusterDescArr)
    arr.reserve(256); // reasonable default
}

ClusterWindRenderer::~ClusterWindRenderer()
{
  cluster_buf.close();
  cluster_buf_prev.close();
}
