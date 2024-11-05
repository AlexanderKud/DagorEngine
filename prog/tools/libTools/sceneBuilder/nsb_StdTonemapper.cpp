// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include <ioSys/dag_dataBlock.h>
#include <sceneBuilder/nsb_StdTonemapper.h>
#include <debug/dag_debug.h>


StaticSceneBuilder::StdTonemapper::StdTonemapper() :
  scaleColor(1, 1, 1),
  gammaColor(1, 1, 1),
  postScaleColor(1, 1, 1),
  scaleMul(1),
  gammaMul(2),
  postScaleMul(0.5f),
  hdrScaleColor(1, 1, 1),
  hdrAmbientColor(0, 0, 0),
  hdrClampColor(1, 1, 1),
  hdrScaleMul(1),
  hdrAmbientMul(0),
  hdrClampMul(1e6f),
  hdrGammaColor(1, 1, 1),
  hdrGammaRangeColor(1, 1, 1),
  hdrGammaMul(1),
  hdrGammaRangeMul(1),
  maxHdrLightmapValue(4)
{
  recalc();
}


void StaticSceneBuilder::StdTonemapper::recalc()
{
  scale = scaleColor * scaleMul;

  postScale = postScaleColor * postScaleMul;

  invGamma = gammaColor * gammaMul;
  for (int i = 0; i < 3; ++i)
    if (invGamma[i] != 0)
      invGamma[i] = 1 / invGamma[i];

  hdrScale = hdrScaleColor * hdrScaleMul;
  hdrAmbient = hdrAmbientColor * hdrAmbientMul;
  hdrClamp = hdrClampColor * hdrClampMul;
  invHdrGamma = hdrGammaColor * hdrGammaMul;
  for (int i = 0; i < 3; ++i)
  {
    if (invHdrGamma[i] != 0)
    {
      invHdrGamma[i] = 1 / invHdrGamma[i];
      processedHdrGammaRange[i] = pow(hdrGammaRangeColor[i] * hdrGammaRangeMul, 1.f - invHdrGamma[i]);
    }
    else
    {
      processedHdrGammaRange[i] = 0.f;
    }
  }
}


void StaticSceneBuilder::StdTonemapper::save(DataBlock &blk) const
{
  blk.setReal("scaleMul", scaleMul);
  blk.setReal("gammaMul", gammaMul);
  blk.setReal("postScaleMul", postScaleMul);

  blk.setE3dcolor("scaleColor", e3dcolor(scaleColor));
  blk.setE3dcolor("gammaColor", e3dcolor(gammaColor));
  blk.setE3dcolor("postScaleColor", e3dcolor(postScaleColor));

  blk.setReal("hdrScaleMul", hdrScaleMul);
  blk.setReal("hdrAmbientMul", hdrAmbientMul);
  blk.setReal("hdrClampMul", hdrClampMul);
  blk.setReal("hdrGammaMul", hdrGammaMul);
  blk.setReal("hdrGammaRangeMul", hdrGammaRangeMul);

  blk.setE3dcolor("hdrScaleColor", e3dcolor(hdrScaleColor));
  blk.setE3dcolor("hdrAmbientColor", e3dcolor(hdrAmbientColor));
  blk.setE3dcolor("hdrClampColor", e3dcolor(hdrClampColor));
  blk.setE3dcolor("hdrGammaColor", e3dcolor(hdrGammaColor));
  blk.setE3dcolor("hdrGammaRangeColor", e3dcolor(hdrGammaRangeColor));

  blk.setReal("maxHdrLightmapValue", maxHdrLightmapValue);
}


void StaticSceneBuilder::StdTonemapper::load(const DataBlock &blk)
{
  scaleMul = blk.getReal("scaleMul", scaleMul);
  gammaMul = blk.getReal("gammaMul", gammaMul);
  postScaleMul = blk.getReal("postScaleMul", postScaleMul);

  scaleColor = color3(blk.getE3dcolor("scaleColor", e3dcolor(scaleColor)));
  gammaColor = color3(blk.getE3dcolor("gammaColor", e3dcolor(gammaColor)));
  postScaleColor = color3(blk.getE3dcolor("postScaleColor", e3dcolor(postScaleColor)));

  hdrScaleMul = blk.getReal("hdrScaleMul", hdrScaleMul);
  hdrAmbientMul = blk.getReal("hdrAmbientMul", hdrAmbientMul);
  hdrClampMul = blk.getReal("hdrClampMul", hdrClampMul);
  hdrGammaMul = blk.getReal("hdrGammaMul", hdrGammaMul);
  hdrGammaRangeMul = blk.getReal("hdrGammaRangeMul", hdrGammaRangeMul);

  hdrScaleColor = color3(blk.getE3dcolor("hdrScaleColor", e3dcolor(hdrScaleColor)));
  hdrAmbientColor = color3(blk.getE3dcolor("hdrAmbientColor", e3dcolor(hdrAmbientColor)));
  hdrClampColor = color3(blk.getE3dcolor("hdrClampColor", e3dcolor(hdrClampColor)));
  hdrGammaColor = color3(blk.getE3dcolor("hdrGammaColor", e3dcolor(hdrGammaColor)));
  hdrGammaRangeColor = color3(blk.getE3dcolor("hdrGammaRangeColor", e3dcolor(hdrGammaRangeColor)));

  maxHdrLightmapValue = blk.getReal("maxHdrLightmapValue", maxHdrLightmapValue);

  recalc();
}

void StaticSceneBuilder::Splitter::save(DataBlock &blk)
{
  blk.setInt("trianglesPerObjectTargeted", trianglesPerObjectTargeted);
  blk.setReal("minRadiusToSplit", minRadiusToSplit);
  blk.setReal("maxCombinedSize", maxCombinedSize);
  blk.setBool("combineObjects", combineObjects);
  blk.setBool("splitObjects", splitObjects);
  blk.setBool("optimizeForVertexCache", optimizeForVertexCache);
  blk.setBool("buildShadowSD", buildShadowSD);
}

void StaticSceneBuilder::Splitter::load(const DataBlock &blk, bool init_def)
{
  if (init_def)
    defaults();

  trianglesPerObjectTargeted = blk.getInt("trianglesPerObjectTargeted", trianglesPerObjectTargeted);
  minRadiusToSplit = blk.getReal("minRadiusToSplit", minRadiusToSplit);
  maxCombinedSize = blk.getReal("maxCombinedSize", maxCombinedSize);
  combineObjects = blk.getBool("combineObjects", combineObjects);
  splitObjects = blk.getBool("splitObjects", splitObjects);
  optimizeForVertexCache = blk.getBool("optimizeForVertexCache", optimizeForVertexCache);
  buildShadowSD = blk.getBool("buildShadowSD", buildShadowSD);
}
