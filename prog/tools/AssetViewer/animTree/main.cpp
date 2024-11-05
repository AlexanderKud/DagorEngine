// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include "animTree.h"

#include <EditorCore/ec_interface.h>

#include <generic/dag_initOnDemand.h>
#include <debug/dag_debug.h>

static InitOnDemand<AnimTreePlugin> plugin;

void init_plugin_anim_tree()
{
  if (!IEditorCoreEngine::get()->checkVersion())
  {
    debug("incorrect version!");
    return;
  }

  ::plugin.demandInit();

  IEditorCoreEngine::get()->registerPlugin(::plugin);
}