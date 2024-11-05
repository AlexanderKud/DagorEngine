// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include "prefabs.h"
#include "pf_cm.h"

#include <libTools/staticGeom/staticGeometryContainer.h>
#include <libTools/staticGeom/geomObject.h>
#include <libTools/staticGeomUi/nodeFlags.h>

#include <propPanel/control/container.h>


//==============================================================================
void PrefabsPlugin::fillPropPanel(PropPanel::ContainerPropertyControl &panel)
{
  panel.setEventHandler(this);

  panel.disableFillAutoResize(); // debug

  PropPanel::ContainerPropertyControl *maxGrp = panel.createGroup(ID_SO_CLIPPING_GRP, "Clipping");

  NodesData data;
  data.useDefault = false;

  const StaticGeometryContainer *cont = geom->getGeometryContainer();
  G_ASSERT(cont);

  NodeParamsTab nodeParamsTab;
  bool hasParamsTab = changedNodes.get(curAssetDagFname, nodeParamsTab);

  for (int i = 0; i < cont->nodes.size(); ++i)
  {
    StaticGeometryNode *node = cont->nodes[i];
    if (node)
    {
      NodesData::NodeFlags flags(*node);
      if (hasParamsTab && nodeParamsTab.size() > i)
      {
        flags.showOccluder = nodeParamsTab[i].showOccluders;
      }

      data.flags.push_back(flags);
    }
  }

  NodeFlagsSettings settings;
  settings.showUseDefault = false;
  settings.disableLinkedRes = false;
  settings.editLodRange = true;
  settings.editLodName = true;

  nodeModif->setSettings(settings);

  nodeModif->fillPanel(panel, data, PID_STATIC_GEOM_GUI_START);

  panel.restoreFillAutoResize(); // debug
}
