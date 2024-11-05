#ifndef DAFX_MODFX_COLOR_HLSL
#define DAFX_MODFX_COLOR_HLSL

DAFX_INLINE
ModfxDeclColorInitRnd ModfxDeclColorInitRnd_load( BufferData_cref buf, uint ofs )
{
#ifdef __cplusplus
  return *(ModfxDeclColorInitRnd*)( buf + ofs );
#else
  ModfxDeclColorInitRnd pp;
  pp.color_min = dafx_load_1ui( buf, ofs );
  pp.color_max = dafx_load_1ui( buf, ofs );
  return pp;
#endif
}

DAFX_INLINE
void modfx_color_init_rnd( BufferData_cref buf, uint ofs, rnd_seed_ref rnd_seed, float4_ref o_color )
{
  ModfxDeclColorInitRnd pp = ModfxDeclColorInitRnd_load( buf, ofs );

  float4 c0 = unpack_e3dcolor_to_n4f( pp.color_min  );
  float4 c1 = unpack_e3dcolor_to_n4f( pp.color_max  );
  o_color = lerp( c0, c1, dafx_frnd( rnd_seed ) );
}

DAFX_INLINE
void modfx_color_init( ModfxParentSimData_cref parent_sdata, BufferData_cref buf, rnd_seed_ref rnd_seed, float4_ref o_color )
{
  if ( parent_sdata.mods_offsets[MODFX_SMOD_COLOR_INIT] )
    o_color = unpack_e3dcolor_to_n4f( dafx_get_1ui( buf, parent_sdata.mods_offsets[MODFX_SMOD_COLOR_INIT] ) );
  else if ( parent_sdata.mods_offsets[MODFX_SMOD_COLOR_INIT_RND] )
    modfx_color_init_rnd( buf, parent_sdata.mods_offsets[MODFX_SMOD_COLOR_INIT_RND], rnd_seed, o_color );
}

DAFX_INLINE
void modfx_color_apply_curve( BufferData_cref buf, uint ofs, float life_k, float4_ref o_color )
{
  float4 mask = unpack_e3dcolor_to_n4f( dafx_load_1ui( buf, ofs ) );
  float curve = modfx_get_1f_curve( buf, ofs, life_k );

  float4 v = ( 1.f - curve ) * mask;

  o_color.x *= 1.f - v.x;
  o_color.y *= 1.f - v.y;
  o_color.z *= 1.f - v.z;
  o_color.w *= 1.f - v.w;
}

DAFX_INLINE
void modfx_color_sim( ModfxParentSimData_cref parent_sdata, BufferData_cref buf,
  rnd_seed_ref rnd_seed, float grad_k, float curve_k, float emm_k, float part_distr_k,
  float4_cref mul_color, float4_ref o_color, float_ref o_emission_fade )
{
  modfx_color_init( parent_sdata, buf, rnd_seed, o_color );

  if ( parent_sdata.mods_offsets[MODFX_SMOD_COLOR_OVER_PART_LIFE_GRAD] )
    o_color = o_color * modfx_get_e3dcolor_grad( buf, parent_sdata.mods_offsets[MODFX_SMOD_COLOR_OVER_PART_LIFE_GRAD], grad_k );

  if ( parent_sdata.mods_offsets[MODFX_SMOD_COLOR_OVER_PART_LIFE_CURVE] )
    modfx_color_apply_curve( buf, parent_sdata.mods_offsets[MODFX_SMOD_COLOR_OVER_PART_LIFE_CURVE], curve_k, o_color );

  if ( parent_sdata.mods_offsets[MODFX_SMOD_COLOR_OVER_PART_IDX_CURVE] )
  {
    modfx_color_apply_curve( buf, parent_sdata.mods_offsets[MODFX_SMOD_COLOR_OVER_PART_IDX_CURVE], part_distr_k, o_color );
  }

  if ( parent_sdata.mods_offsets[MODFX_SMOD_COLOR_EMISSION_OVER_PART_LIFE] )
    o_emission_fade = modfx_get_1f_curve( buf, parent_sdata.mods_offsets[MODFX_SMOD_COLOR_EMISSION_OVER_PART_LIFE], emm_k );

  o_color.x *= mul_color.x;
  o_color.y *= mul_color.y;
  o_color.z *= mul_color.z;
  o_color.w *= mul_color.w;
}

DAFX_INLINE
ModfxDeclAlphaByVelocity ModfxDeclAlphaByVelocity_load( BufferData_cref buf, uint ofs )
{
#ifdef __cplusplus
  return *(ModfxDeclAlphaByVelocity*)(buf + ofs);
#else
  ModfxDeclAlphaByVelocity pp;
  pp.vel_max = dafx_load_1f( buf, ofs );
  pp.vel_min = dafx_load_1f( buf, ofs );
  pp.inv_vel_diff = dafx_load_1f( buf, ofs );
  pp.neg_minvel_div_by_diff = dafx_load_1f( buf, ofs );
  return pp;
#endif
}

DAFX_INLINE
void modfx_color_alpha_by_velocity(
  ModfxParentSimData_cref parent_sdata, ModfxParentRenData_cref parent_rdata,
  BufferData_cref buf, uint ofs, float4_ref o_color, float vel )
{
  ModfxDeclAlphaByVelocity pp = ModfxDeclAlphaByVelocity_load( buf, ofs );
  uint emitter_ofs = parent_sdata.mods_offsets[MODFX_SMOD_VELOCITY_INIT_ADD_STATIC_VEC];

  if ( FLAG_ENABLED( parent_sdata.flags, MODFX_SFLAG_ALPHA_BY_EMITTER_VELOCITY ) &&
       MOD_ENABLED( parent_sdata.mods, MODFX_SMOD_VELOCITY_INIT_ADD_STATIC_VEC ) )
    vel = length(dafx_get_3f( buf, emitter_ofs ));

  // alpha is the inverse lerp for a velocity between
  // vel_min and vel_max, and clamped to 0 - 1 range
  // alpha = (vel - vel_min) / (vel_max - vel_min)
  float alpha = saturate(pp.inv_vel_diff * vel + pp.neg_minvel_div_by_diff);
  float weight = 1.f - alpha;

  if (parent_sdata.mods_offsets[MODFX_SMOD_ALPHA_BY_VELOCITY_CURVE])
  {
    weight = modfx_get_1f_curve(
      buf, parent_sdata.mods_offsets[MODFX_SMOD_ALPHA_BY_VELOCITY_CURVE], alpha );
  }

  o_color.w *= weight;
  if ( FLAG_ENABLED( parent_rdata.flags, MODFX_RFLAG_BLEND_PREMULT ) )
  {
    o_color.x *= weight;
    o_color.y *= weight;
    o_color.z *= weight;
  }
}

#endif
