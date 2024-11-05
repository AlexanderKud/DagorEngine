// Copyright (C) Gaijin Games KFT.  All rights reserved.

#include <render/fxaa.h>
#include <shaders/dag_shaders.h>
#include <drv/3d/dag_driver.h>
#include <3d/dag_texMgr.h>
#include <shaders/dag_postFxRenderer.h>
#include <startup/dag_globalSettings.h>
#include <ioSys/dag_dataBlock.h>
#include <math/dag_color.h>


static int resolution_params_id = -1;
Antialiasing::Antialiasing()
{
  sourceColorTexVarId = get_shader_glob_var_id("source_color_tex", true);
  sourceDepthTexVarId = get_shader_glob_var_id("source_depth_tex", true);
  d3d::SamplerInfo smpInfo;
  smpInfo.address_mode_u = smpInfo.address_mode_v = smpInfo.address_mode_w = d3d::AddressMode::Clamp;
  smpInfo.filter_mode = d3d::FilterMode::Linear;
  ShaderGlobal::set_sampler(get_shader_glob_var_id("source_color_tex_samplerstate", true), d3d::request_sampler(smpInfo));

  antialiasingRenderer = new PostFxRenderer;
  antialiasingRenderer->init("antialiasing");

  const DataBlock *antialiasingBlk = ::dgs_get_game_params()->getBlockByNameEx("antialiasing");
  resolution_params_id = ::get_shader_glob_var_id("resolution", true);
  int aa_varid = ::get_shader_glob_var_id("fxaa_params", true);
  if (VariableMap::isGlobVariablePresent(aa_varid))
    ShaderGlobal::set_color4_fast(aa_varid, antialiasingBlk->getReal("fxaaQualitySubpix", 0.5f),
      antialiasingBlk->getReal("fxaaQualityEdgeThreshold", 0.166f), antialiasingBlk->getReal("fxaaQualityEdgeThresholdMin", 0.0833f),
      0.f);

  fxaaHighQuality = antialiasingBlk->getInt("fxaaQuality", 29);
  ShaderGlobal::set_int(get_shader_variable_id("fxaa_quality"), fxaaHighQuality);
}


Antialiasing::~Antialiasing() { del_it(antialiasingRenderer); }


void Antialiasing::apply(TEXTUREID source_color_tex_id, TEXTUREID source_depth_tex_id, const Point4 &tc_scale_offset)
{
  if (resolution_params_id >= 0)
  {
    Texture *sourceTex = (Texture *)(::acquire_managed_tex(source_color_tex_id));
    G_ASSERT(sourceTex);

    TextureInfo info;
    sourceTex->getinfo(info);
    release_managed_tex(source_color_tex_id);

    ShaderGlobal::set_color4_fast(resolution_params_id, info.w, info.h, 1.f / info.w, 1.f / info.h);
  }

  static int fxaaTcScaleOffsetVarId = get_shader_glob_var_id("fxaa_tc_scale_offset");
  ShaderGlobal::set_color4(fxaaTcScaleOffsetVarId, Color4::xyzw(tc_scale_offset));

  // One pass.
  if (sourceDepthTexVarId >= 0)
    ShaderGlobal::set_texture_fast(sourceDepthTexVarId, source_depth_tex_id);

  if (sourceColorTexVarId >= 0)
    ShaderGlobal::set_texture_fast(sourceColorTexVarId, source_color_tex_id);
  antialiasingRenderer->render();

  if (sourceDepthTexVarId >= 0)
    ShaderGlobal::set_texture_fast(sourceDepthTexVarId, BAD_TEXTUREID);

  if (sourceColorTexVarId >= 0)
    ShaderGlobal::set_texture_fast(sourceColorTexVarId, BAD_TEXTUREID);
}


void Antialiasing::setType(FxaaType type)
{
  static int fxaaQualityVarId = get_shader_variable_id("fxaa_quality");
  ShaderGlobal::set_int(fxaaQualityVarId, type == AA_TYPE_NONE ? -1 : (type == AA_TYPE_LOW_FXAA ? 0 : fxaaHighQuality));
}

void Antialiasing::setColorMul(const Color4 &color)
{
  static int fxaa_color_mulVarId = get_shader_variable_id("fxaa_color_mul");
  ShaderGlobal::set_color4(fxaa_color_mulVarId, color);
}
