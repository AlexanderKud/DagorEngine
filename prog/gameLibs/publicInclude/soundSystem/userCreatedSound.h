//
// Dagor Engine 6.5 - Game Libraries
// Copyright (C) Gaijin Games KFT.  All rights reserved.
//
#pragma once

#include <stdint.h>

namespace sndsys
{
typedef void (*CustomStereoSoundCb)(float *out_buf, int frequency, int channels, int samples);

bool start_custom_sound(int frequency, int channels, CustomStereoSoundCb fill_buffer_cb);
void stop_custom_sound();
uint64_t get_custom_sound_play_pos(); // in samples
} // namespace sndsys
