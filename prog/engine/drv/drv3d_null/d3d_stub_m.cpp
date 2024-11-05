// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include <drv/3d/dag_lock.h>
#include <drv/3d/dag_info.h>

D3dInterfaceTable d3di;

bool d3d::is_inited() { return false; }
DriverCode d3d::get_driver_code() { return DriverCode::make(d3d::null); }

static void *na_func()
{
  DAG_FATAL("D3DI function not implemented");
  return nullptr;
}
bool d3d::fill_interface_table(D3dInterfaceTable &d3dit)
{
  void *(**funcTbl)() = (void *(**)()) & d3dit;
  for (int i = 0; i < sizeof(d3di) / sizeof(*funcTbl); i++)
    funcTbl[i] = &na_func;
  return false;
}
bool d3d::init_driver()
{
  fill_interface_table(d3di);
  return false;
}

d3d::GpuAutoLock::GpuAutoLock() = default;
d3d::GpuAutoLock::~GpuAutoLock() = default;
