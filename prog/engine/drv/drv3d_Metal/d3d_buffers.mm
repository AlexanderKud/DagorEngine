// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include <generic/dag_tab.h>
#include <drv/3d/dag_draw.h>
#include <drv/3d/dag_vertexIndexBuffer.h>
#include <drv/3d/dag_shaderConstants.h>
#include <drv/3d/dag_buffers.h>
#include <drv/3d/dag_commands.h>
#include <math/dag_TMatrix.h>
#include <math/dag_TMatrix4.h>
#include <debug/dag_debug.h>

#include <vecmath/dag_vecMath.h>
#include <math/dag_vecMathCompatibility.h>
#include <util/dag_string.h>
#include <util/dag_watchdog.h>

#include <drv/3d/dag_driver.h>

#include <validate_sbuf_flags.h>

#include "render.h"
#include "drv_log_defs.h"

using namespace drv3d_metal;

extern bool dirty_render_target();

VDECL d3d::create_vdecl(VSDTYPE* d)
{
  return render.createVDdecl(d);
}

void d3d::delete_vdecl(VDECL vdecl)
{
  render.deleteVDecl(vdecl);
}

bool d3d::setvdecl(VDECL vdecl)
{
  render.setVDecl(vdecl);
  return true;
}

Sbuffer *d3d::create_vb(int size, int flg, const char* name)
{
  flg |= SBCF_BIND_VERTEX;
  validate_sbuffer_flags(flg, name);
  return new Buffer(size, 0, flg, 0, name);
}

Sbuffer *d3d::create_ib(int size, int flg, const char *name)
{
  flg |= SBCF_BIND_INDEX;
  validate_sbuffer_flags(flg, name);
  return new Buffer(size, 0, flg, 0, name);
}

Sbuffer *d3d::create_sbuffer(int struct_size, int elements, unsigned flags, unsigned format, const char* name)
{
  validate_sbuffer_flags(flags, name);
  return new Buffer(elements, struct_size, flags, format, name);
}

bool set_buffer_ex(unsigned shader_stage, BufferType buf_type, int slot, Buffer *vb, int offset, int stride)
{
  Buffer* bufMetal = (Buffer*)vb;
  // yet another special case, don't invalidate readback frame if the buffer has readback flag
  // it should be read back by async readback lock magic call
  if (bufMetal && buf_type == RW_BUFFER && (bufMetal->bufFlags & SBCF_USAGE_READ_BACK) == 0)
    bufMetal->last_locked_submit = render.submits_scheduled;
  render.setBuff(shader_stage, buf_type, slot, bufMetal, offset, stride);

  return true;
}

bool d3d::setvsrc_ex(int slot, Sbuffer *vb, int offset, int stride)
{
  return set_buffer_ex(STAGE_VS, GEOM_BUFFER, slot, (Buffer*)vb, offset, stride);
}

bool d3d::set_const_buffer(unsigned stage, unsigned slot, Sbuffer* buffer, uint32_t consts_offset, uint32_t consts_size)
{
  G_ASSERT_RETURN(!consts_offset && !consts_size, false); //not implemented, not tested
  return set_buffer_ex(stage, CONST_BUFFER, slot, (Buffer*)buffer, 0, 0);
}

bool d3d::set_buffer(unsigned shader_stage, unsigned slot, Sbuffer *buffer)
{
  int offset = 0;
  Buffer* buf = (Buffer*)buffer;
  return set_buffer_ex(shader_stage, STRUCT_BUFFER, slot, buf, offset, 0);
}

bool d3d::set_rwbuffer(unsigned shader_stage, unsigned slot, Sbuffer *buffer)
{
  return set_buffer_ex(shader_stage, RW_BUFFER, slot, (Buffer*)buffer, 0, 0);
}

bool d3d::setind(Sbuffer *ib)
{
  render.setIBuff((Buffer*)ib);

  return true;
}

static inline uint32_t nprim_to_nverts(uint32_t prim_type, uint32_t numprim)
{
  //table look-up: 4 bits per entry [2b mul 2bit add]
  static const uint64_t table =
    (0x0ULL << (4 * PRIM_POINTLIST))  // 1+0 00/00
    | (0x4ULL << (4 * PRIM_LINELIST))   // 2+0 01/00
    | (0x1ULL << (4 * PRIM_LINESTRIP))  // 1+1 00/01
    | (0x8ULL << (4 * PRIM_TRILIST))  // 3+0 10/00
    | (0x2ULL << (4 * PRIM_TRISTRIP))   // 1+2 00/10
    | (0x8ULL << (4 * PRIM_TRIFAN))   // 1+2 00/10
                      //| (0xcLL << 4*PRIM_QUADLIST)   // 4+0 11/00
                      //| (0x3LL << 4*PRIM_QUADSTRIP);   // 1+3 00/11
    ;

  const uint32_t code = uint32_t((table >> (prim_type * 4)) & 0x0f);
  return numprim * ((code >> 2) + 1) + (code & 3);
}

void check_primtype(const int prim_type)
{
  G_ASSERT(prim_type >= 0 && prim_type < PRIM_COUNT && prim_type != PRIM_4_CONTROL_POINTS);
}

bool d3d::draw_base(int prim_type, int start_vertex, int numprim, uint32_t num_inst, uint32_t start_instance)
{
  dirty_render_target();
  check_primtype(prim_type);
  return render.draw(prim_type, start_vertex, nprim_to_nverts(prim_type, numprim), num_inst, start_instance);
}

bool d3d::drawind_base(int prim_type, int startind, int numprim, int base_vertex, uint32_t num_inst, uint32_t start_instance)
{
  dirty_render_target();
  check_primtype(prim_type);
  return render.drawIndexed(prim_type, startind, nprim_to_nverts(prim_type, numprim), base_vertex, num_inst, start_instance);
}

bool d3d::draw_indirect(int prim_type, Sbuffer* buffer, uint32_t offset)
{
  dirty_render_target();
  check_primtype(prim_type);
  return render.drawIndirect(prim_type, buffer, offset);
}

bool d3d::draw_indexed_indirect(int prim_type, Sbuffer* buffer, uint32_t offset)
{
  dirty_render_target();
  check_primtype(prim_type);
  return render.drawIndirectIndexed(prim_type, buffer, offset);
}

bool d3d::multi_draw_indirect(int prim_type, Sbuffer *args, uint32_t draw_count, uint32_t stride_bytes, uint32_t byte_offset)
{
  bool res = true;
  for (int i = 0; i < draw_count; ++i, byte_offset+=stride_bytes)
    res &= d3d::draw_indirect(prim_type, args, byte_offset);
  return res;
}

bool d3d::multi_draw_indexed_indirect(int prim_type, Sbuffer *args, uint32_t draw_count, uint32_t stride_bytes, uint32_t byte_offset)
{
  bool res = true;
  for (int i = 0; i < draw_count; ++i, byte_offset+=stride_bytes)
    res &= d3d::draw_indexed_indirect(prim_type, args, byte_offset);
  return res;
}

bool d3d::draw_up(int prim_type, int numprim, const void* ptr, int stride_bytes)
{
  dirty_render_target();
  check_primtype(prim_type);
  return render.drawUP(prim_type, nprim_to_nverts(prim_type, numprim), NULL, ptr, nprim_to_nverts(prim_type, numprim), stride_bytes);
}

/// draw indexed primitives from user pointer (rather slow)
bool d3d::drawind_up(int prim_type, int minvert, int numvert, int numprim, const uint16_t *ind,
                     const void* ptr, int stride_bytes)
{
  dirty_render_target();
  check_primtype(prim_type);
  return render.drawUP(prim_type, nprim_to_nverts(prim_type, numprim), ind, ptr, numvert, stride_bytes);
}
#define IMMEDIATE_CB_NAMESPACE namespace drv3d_metal

bool d3d::set_immediate_const(unsigned shader_stage, const uint32_t *data, unsigned num_words)
{
  render.setImmediateConst(shader_stage, data, num_words);

  return true;
}
