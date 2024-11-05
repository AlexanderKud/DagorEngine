//
// Dagor Engine 6.5 - Game Libraries
// Copyright (C) Gaijin Games KFT.  All rights reserved.
//
#pragma once

class DataBlock;

namespace physmat
{
struct SoftMaterialProps
{
  float physViscosity = 1.f;
  float collisionForce = 1.f;

  void load(const DataBlock *blk);
  static const SoftMaterialProps *get_props(int prop_id);
  static void register_props_class();
  static bool can_load(const DataBlock *);
};
}; // namespace physmat
