//
// Dagor Tech 6.5
// Copyright (C) Gaijin Games KFT.  All rights reserved.
//
#pragma once

#include <libTools/pageAsg/asg_data.h>
#include <generic/dag_tab.h>
#include <util/dag_string.h>
#include <math/dag_math3d.h>


// base interface for graph node
class AsgGraphNode
{
protected:
  struct BranchCondition
  {
    String cond;
    int targetStateId;
    AnimGraphBranchType branchType;
    int ordinal; // index in AnimGraphState's Tab of conditions
  };

  IAsgGraphNodeManager &idx;
  Tab<BranchCondition> baseConditions;
  int inLinkCnt;
  int stateId;

  bool uplink, isGroup;

public:
  AsgGraphNode(IAsgGraphNodeManager &_idx, const AnimGraphState &gs, int state_id, bool _uplink, bool is_group);

  void resetInCount();
  void addLink(AsgGraphNode *dest, AnimGraphBranchType btype, const char *cond = NULL);

  int getStateId() { return stateId; }
};
