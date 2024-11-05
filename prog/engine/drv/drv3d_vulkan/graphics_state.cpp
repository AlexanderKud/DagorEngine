// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include "pipeline.h"
#include "graphics_state2.h"
#include "execution_state.h"
#include "pipeline_state.h"

namespace drv3d_vulkan
{

VULKAN_TRACKED_STATE_FIELD_REF(BackGraphicsState, graphics, ExecutionStateStorage);
VULKAN_TRACKED_STATE_FIELD_REF(FrontGraphicsState, graphics, PipelineStateStorage);

template <>
void BackGraphicsState::reset(ExecutionStateStorage &)
{
  TrackedState::reset();
  pipelineState.reset();
  framebufferState.reset();
}

} // namespace drv3d_vulkan

using namespace drv3d_vulkan;

void BackGraphicsStateStorage::makeDirty() { dynamic.makeDirty(); }

void FrontGraphicsStateStorage::makeDirty()
{
  vertexBuffers.makeDirty();
  vertexBufferStrides.makeDirty();
  bindsPS.makeDirty();
  bindsVS.makeDirty();
  framebuffer.makeDirty();
  renderpass.makeDirty();
}

void BackGraphicsStateStorage::clearDirty() { dynamic.clearDirty(); }

void FrontGraphicsStateStorage::clearDirty()
{
  vertexBuffers.clearDirty();
  vertexBufferStrides.clearDirty();
  bindsPS.clearDirty();
  bindsVS.clearDirty();
  framebuffer.clearDirty();
  renderpass.clearDirty();
}