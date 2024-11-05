//
// Dagor Engine 6.5 - Game Libraries
// Copyright (C) Gaijin Games KFT.  All rights reserved.
//
#pragma once

#include <EASTL/unique_ptr.h>

class ShaderElement;
class ShaderMaterial;

class DebugLightProbeSpheres
{
  eastl::unique_ptr<ShaderMaterial> sphereMat;
  ShaderElement *sphereElem = nullptr;

  uint32_t spheresCount = 0;

public:
  DebugLightProbeSpheres(const char *shaderName);

  void setSpheresCount(const int count) { spheresCount = count; }
  void render() const;
};
