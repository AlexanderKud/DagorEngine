// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include "joy_classdrv.h"
#include <drv/hid/dag_hiCreate.h>
#include <debug/dag_debug.h>

using namespace HumanInput;

IGenJoystickClassDrv *HumanInput::createJoystickClassDriver(bool exclude_xinput, bool remap_360)
{
  HidJoystickClassDriver *cd = new (inimem) HidJoystickClassDriver(exclude_xinput, remap_360);
  if (!cd->init())
  {
    delete cd;
    cd = NULL;
  }
  return cd;
}
