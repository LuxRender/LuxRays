#include <string>
namespace slg { namespace ocl {
std::string KernelSource_volume_funcs = 
"#line 2 \"volume_funcs.cl\"\n"
"\n"
"/***************************************************************************\n"
" * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *\n"
" *                                                                         *\n"
" *   This file is part of LuxRender.                                       *\n"
" *                                                                         *\n"
" * Licensed under the Apache License, Version 2.0 (the \"License\");         *\n"
" * you may not use this file except in compliance with the License.        *\n"
" * You may obtain a copy of the License at                                 *\n"
" *                                                                         *\n"
" *     http://www.apache.org/licenses/LICENSE-2.0                          *\n"
" *                                                                         *\n"
" * Unless required by applicable law or agreed to in writing, software     *\n"
" * distributed under the License is distributed on an \"AS IS\" BASIS,       *\n"
" * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*\n"
" * See the License for the specific language governing permissions and     *\n"
" * limitations under the License.                                          *\n"
" ***************************************************************************/\n"
"\n"
"#if defined(PARAM_HAS_VOLUMES)\n"
"\n"
"//------------------------------------------------------------------------------\n"
"// ClearVolume scatter\n"
"//------------------------------------------------------------------------------\n"
"\n"
"float3 ClearVolume_SigmaA(__global Volume *vol, __global HitPoint *hitPoint\n"
"	TEXTURES_PARAM_DECL) {\n"
"	const float3 sigmaA = Texture_GetSpectrumValue(&texs[vol->volume.clear.sigmaATexIndex], hitPoint\n"
"		TEXTURES_PARAM);\n"
"			\n"
"	return clamp(sigmaA, 0.f, INFINITY);\n"
"}\n"
"\n"
"float3 ClearVolume_SigmaS(__global Volume *vol, __global HitPoint *hitPoint\n"
"	TEXTURES_PARAM_DECL) {\n"
"	return BLACK;\n"
"}\n"
"\n"
"float3 ClearVolume_SigmaT(__global Volume *vol, __global HitPoint *hitPoint\n"
"	TEXTURES_PARAM_DECL) {\n"
"	return\n"
"			ClearVolume_SigmaA(vol, hitPoint\n"
"				TEXTURES_PARAM) +\n"
"			ClearVolume_SigmaS(vol, hitPoint\n"
"				TEXTURES_PARAM);\n"
"}\n"
"\n"
"float ClearVolume_Scatter(__global Volume *vol,\n"
"#if !defined(BSDF_INIT_PARAM_MEM_SPACE_PRIVATE)\n"
"		__global\n"
"#endif\n"
"		Ray *ray, const float hitT,\n"
"#if defined(PARAM_HAS_PASSTHROUGH)\n"
"		const float passThroughEvent,\n"
"#endif\n"
"		const bool scatteredStart, float3 *connectionThroughput,\n"
"		float3 *connectionEmission, __global HitPoint *tmpHitPoint\n"
"		TEXTURES_PARAM_DECL) {\n"
"	// Initialize tmpHitPoint\n"
"#if !defined(BSDF_INIT_PARAM_MEM_SPACE_PRIVATE)\n"
"	const float3 rayOrig = VLOAD3F(&ray->o.x);\n"
"	const float3 rayDir = VLOAD3F(&ray->d.x);\n"
"#else\n"
"	const float3 rayOrig = (float3)(ray->o.x, ray->o.y, ray->o.z);\n"
"	const float3 rayDir = (float3)(ray->d.x, ray->d.y, ray->d.z);\n"
"#endif\n"
"	VSTORE3F(rayDir, &tmpHitPoint->fixedDir.x);\n"
"	VSTORE3F(rayOrig, &tmpHitPoint->p.x);\n"
"	VSTORE2F((float2)(0.f, 0.f), &tmpHitPoint->uv.u);\n"
"	VSTORE3F(-rayDir, &tmpHitPoint->geometryN.x);\n"
"	VSTORE3F(-rayDir, &tmpHitPoint->shadeN.x);\n"
"#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR) || defined(PARAM_ENABLE_TEX_HITPOINTGREY)\n"
"	VSTORE2F(WHITE, &tmpHitPoint->color.c);\n"
"#endif\n"
"#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)\n"
"	VSTORE2F(1.f, &tmpHitPoint->alpha);\n"
"#endif\n"
"#if defined(PARAM_HAS_PASSTHROUGH)\n"
"	tmpHitPoint->passThroughEvent = passThroughEvent;\n"
"#endif\n"
"	tmpHitPoint->interiorVolumeIndex = NULL_INDEX;\n"
"	tmpHitPoint->exteriorVolumeIndex = NULL_INDEX;\n"
"	tmpHitPoint->intoObject = true;\n"
"\n"
"	const float distance = hitT - ray->mint;	\n"
"	float3 transmittance = WHITE;\n"
"\n"
"	const float3 sigma = ClearVolume_SigmaT(vol, tmpHitPoint\n"
"			TEXTURES_PARAM);\n"
"	if (!Spectrum_IsBlack(sigma)) {\n"
"		const float3 tau = clamp(distance * sigma, 0.f, INFINITY);\n"
"		transmittance = Spectrum_Exp(-tau);\n"
"	}\n"
"\n"
"	// Apply volume transmittance\n"
"	*connectionThroughput *= transmittance;\n"
"\n"
"	// Apply volume emission\n"
"	const uint emiTexIndex = vol->volume.volumeEmissionTexIndex;\n"
"	if (emiTexIndex != NULL_INDEX) {\n"
"		const float3 emiTex = Texture_GetSpectrumValue(&texs[emiTexIndex], tmpHitPoint\n"
"			TEXTURES_PARAM);\n"
"		*connectionEmission += *connectionThroughput * distance * clamp(emiTex, 0.f, INFINITY);\n"
"	}\n"
"\n"
"	return -1.f;\n"
"}\n"
"\n"
"//------------------------------------------------------------------------------\n"
"// Volume scatter\n"
"//------------------------------------------------------------------------------\n"
"\n"
"float Volume_Scatter(__global Volume *vol,\n"
"#if !defined(BSDF_INIT_PARAM_MEM_SPACE_PRIVATE)\n"
"		__global\n"
"#endif\n"
"		Ray *ray, const float hitT, const float passThrough,\n"
"		const bool scatteredStart, float3 *connectionThroughput,\n"
"		float3 *connectionEmission, __global HitPoint *tmpHitPoint\n"
"		TEXTURES_PARAM_DECL) {\n"
"	switch (vol->type) {\n"
"		case CLEAR_VOL:\n"
"			ClearVolume_Scatter(vol, ray, hitT, passThrough, scatteredStart,\n"
"					connectionThroughput, connectionEmission, tmpHitPoint\n"
"					TEXTURES_PARAM);\n"
"		default:\n"
"			return -1.f;\n"
"	}\n"
"}\n"
"\n"
"#endif\n"
; } }