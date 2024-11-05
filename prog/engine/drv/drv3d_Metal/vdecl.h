// Copyright (C) Gaijin Games KFT.  All rights reserved.
#pragma once

#include <drv/3d/dag_driver.h>
#include <drv/3d/dag_commands.h>

#import <Metal/Metal.h>
#import <simd/simd.h>
#import <MetalKit/MetalKit.h>

namespace drv3d_metal
{
class VDecl
{
public:
  MTLVertexDescriptor *vertexDescriptor;

  int num_location;

  struct Location
  {
    int offset;
    int vsdr;
    int size;
    int stream;
  };

  Location locations[16];

  int num_streams;
  int instanced_mask = 0;

  uint64_t hash;

  VDecl(VSDTYPE *d);
  void release();
};
} // namespace drv3d_metal
