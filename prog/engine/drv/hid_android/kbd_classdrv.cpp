// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include "kbd_classdrv.h"
#include "kbd_device.h"
#include <drv/hid/dag_hiGlobals.h>
#include <drv/hid/dag_hiCreate.h>
#include <osApiWrappers/dag_progGlobals.h>

using namespace HumanInput;

IGenKeyboardClassDrv *HumanInput::createWinKeyboardClassDriver()
{
  memset(&raw_state_kbd, 0, sizeof(raw_state_kbd));
  HumanInput::stg_kbd.enabled = HumanInput::stg_kbd.present = true;

  ScreenKeyboardClassDriver *cd = new (inimem) ScreenKeyboardClassDriver;
  if (!cd->init())
  {
    delete cd;
    cd = NULL;
  }
  return cd;
}


bool ScreenKeyboardClassDriver::init()
{
  stg_kbd.enabled = false;

  refreshDeviceList();
  if (device)
    enable(true);
  return true;
}

void ScreenKeyboardClassDriver::destroyDevices()
{
  unacquireDevices();
  if (device)
    delete device;
  device = NULL;
}

// generic hid class driver interface
void ScreenKeyboardClassDriver::enable(bool en)
{
  enabled = en;
  stg_kbd.enabled = en;
}
void ScreenKeyboardClassDriver::destroy()
{
  destroyDevices();
  stg_kbd.present = false;

  delete this;
}

// generic keyboard class driver interface
void ScreenKeyboardClassDriver::useDefClient(IGenKeyboardClient *cli)
{
  defClient = cli;
  if (device)
    device->setClient(defClient);
}

void ScreenKeyboardClassDriver::refreshDeviceList()
{
  destroyDevices();

  bool was_enabled = enabled;
  enable(false);

  device = new ScreenKeyboardDevice;

  // update global settings
  if (device)
  {
    stg_kbd.present = true;
    enable(was_enabled);
  }
  else
  {
    stg_kbd.present = false;
    stg_kbd.enabled = false;
  }

  useDefClient(defClient);
  acquireDevices();
}

bool HumanInput::keyboard_has_ime_layout() { return false; }
