//
// Dagor Tech 6.5
// Copyright (C) Gaijin Games KFT.  All rights reserved.
//
#pragma once

class IScriptPanelTargetCB
{
public:
  virtual const char *getTarget(const char old_choise[], const char type[], const char filter[]) = 0;
  virtual const char *validateTarget(const char old_choise[], const char type[]) = 0;
};


class IScriptPanelEventHandler
{
public:
  virtual void onScriptPanelChange() = 0;
};
