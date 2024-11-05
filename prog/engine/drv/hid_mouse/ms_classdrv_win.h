// Copyright (C) Gaijin Games KFT.  All rights reserved.
#pragma once

#include <drv/hid/dag_hiPointing.h>
#include "ms_device_win.h"

namespace HumanInput
{
class WinMouseDevice;


class WinMouseClassDriver : public IGenPointingClassDrv
{
  unsigned prevJoyButtons;

public:
  WinMouseClassDriver() : device(NULL), enabled(false), defClient(NULL), prevJoyButtons(0) {}
  ~WinMouseClassDriver() { destroyDevices(); }

  bool init();
  void destroyDevices();

  // generic hid class driver interface
  virtual void enable(bool en);
  virtual void acquireDevices() {}
  virtual void unacquireDevices() {}
  virtual void destroy();

  virtual void refreshDeviceList();

  virtual int getDeviceCount() const { return device ? 1 : 0; }
  virtual void updateDevices();

  // generic keyboard class driver interface
  IGenPointing *getDevice(int idx) const override;
  void useDefClient(IGenPointingClient *cli) override;

protected:
  WinMouseDevice *device;
  IGenPointingClient *defClient;
  bool enabled;

private:
  void onButtonUp(int btn_id);
  void onButtonDown(int btn_id);
};
} // namespace HumanInput
