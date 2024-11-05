//
// Dagor Engine 6.5 - Game Libraries
// Copyright (C) Gaijin Games KFT.  All rights reserved.
//
#pragma once

class Point2;
class IGenAudioSystem;
namespace Sqrat
{
class Object;
}


namespace darg
{

struct IUiSoundPlayer
{
  virtual void play(const char *name, const Sqrat::Object &params, float volume, const Point2 *pos) = 0;

  virtual IGenAudioSystem *getAudioSystem() = 0;
};


extern IUiSoundPlayer *ui_sound_player;

} // namespace darg
