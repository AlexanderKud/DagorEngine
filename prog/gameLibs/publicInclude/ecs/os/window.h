//
// Dagor Engine 6.5 - Game Libraries
// Copyright (C) Gaijin Games KFT.  All rights reserved.
//
#pragma once

#include <daECS/core/entityManager.h>
#include <daECS/core/event.h>

ECS_BROADCAST_EVENT_TYPE(EventWindowActivated);
ECS_BROADCAST_EVENT_TYPE(EventWindowDeactivated);

namespace ecs_os
{

void init_window_handler();
void cleanup_window_handler();

} // namespace ecs_os
