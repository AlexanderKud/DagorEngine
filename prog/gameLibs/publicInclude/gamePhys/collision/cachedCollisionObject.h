//
// Dagor Engine 6.5 - Game Libraries
// Copyright (C) Gaijin Games KFT.  All rights reserved.
//
#pragma once

#include <gamePhys/collision/collisionInfo.h>
#include <rendInst/rendInstGen.h>

struct CachedCollisionObjectInfo : public gamephys::CollisionObjectInfo
{
  rendinst::RendInstDesc riDesc;
  float timeToLive;
  float originalThreshold;
  float thresImpulse;
  bool alive;
  float atTime;

  CachedCollisionObjectInfo(float impulse, const rendinst::RendInstDesc &ri_desc, float at_time) :
    riDesc(ri_desc), timeToLive(1.f), originalThreshold(impulse), thresImpulse(impulse), alive(true), atTime(at_time)
  {}

  virtual ~CachedCollisionObjectInfo() {}

  virtual float onImpulse(float impulse, const Point3 & /*dir*/, const Point3 & /*pos*/, float /*point_vel*/,
    const Point3 & /*collision_normal*/, uint32_t flags = CIF_NONE, int32_t /* user_data */ = -1,
    gamephys::ImpulseLogFunc /*log_func*/ = nullptr) override
  {
    if (flags & CIF_NO_DAMAGE)
      return impulse;

    timeToLive = 1.f;
    return 0.f;
  }

  float getRemainingImpulse() const override { return thresImpulse; }

  bool update(float dt)
  {
    if (alive)
      thresImpulse = min(originalThreshold, thresImpulse + originalThreshold * dt * 0.4f);
    timeToLive -= dt;
    return timeToLive > 0.f && alive;
  }
};
