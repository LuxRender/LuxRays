#include <string>
namespace slg { namespace ocl {
std::string KernelSource_bsdfutils_funcs = 
"#line 2 \"bsdfutils_funcs.cl\"\n"
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
"BSDFEvent BSDF_GetEventTypes(__global BSDF *bsdf\n"
"		MATERIALS_PARAM_DECL) {\n"
"	return Material_GetEventTypes(&mats[bsdf->materialIndex]\n"
"			MATERIALS_PARAM);\n"
"}\n"
"\n"
"bool BSDF_IsDelta(__global BSDF *bsdf\n"
"		MATERIALS_PARAM_DECL) {\n"
"	return Material_IsDelta(&mats[bsdf->materialIndex]\n"
"			MATERIALS_PARAM);\n"
"}\n"
"\n"
"uint BSDF_GetMaterialID(__global BSDF *bsdf\n"
"		MATERIALS_PARAM_DECL) {\n"
"	return mats[bsdf->materialIndex].matID;\n"
"}\n"
"\n"
"uint BSDF_GetLightID(__global BSDF *bsdf\n"
"		MATERIALS_PARAM_DECL) {\n"
"	return mats[bsdf->materialIndex].lightID;\n"
"}\n"
"\n"
"#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)\n"
"bool BSDF_IsLightSource(__global BSDF *bsdf) {\n"
"	return (bsdf->triangleLightSourceIndex != NULL_INDEX);\n"
"}\n"
"\n"
"float3 BSDF_GetEmittedRadiance(__global BSDF *bsdf, float *directPdfA\n"
"		LIGHTS_PARAM_DECL) {\n"
"	const uint triangleLightSourceIndex = bsdf->triangleLightSourceIndex;\n"
"	if (triangleLightSourceIndex == NULL_INDEX)\n"
"		return BLACK;\n"
"	else\n"
"		return IntersectableLight_GetRadiance(&lights[triangleLightSourceIndex],\n"
"				&bsdf->hitPoint, directPdfA\n"
"				LIGHTS_PARAM);\n"
"}\n"
"#endif\n"
"\n"
"#if defined(PARAM_HAS_PASSTHROUGH)\n"
"float3 BSDF_GetPassThroughTransparency(__global BSDF *bsdf\n"
"		MATERIALS_PARAM_DECL) {\n"
"	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, VLOAD3F(&bsdf->hitPoint.fixedDir.x));\n"
"\n"
"	return Material_GetPassThroughTransparency(&mats[bsdf->materialIndex],\n"
"			&bsdf->hitPoint, localFixedDir, bsdf->hitPoint.passThroughEvent\n"
"			MATERIALS_PARAM);\n"
"}\n"
"#endif\n"
"\n"
"#if defined(PARAM_HAS_VOLUMES)\n"
"uint BSDF_GetMaterialInteriorVolume(__global BSDF *bsdf\n"
"		MATERIALS_PARAM_DECL) {\n"
"	return Material_GetInteriorVolume(&mats[bsdf->materialIndex], &bsdf->hitPoint\n"
"#if defined(PARAM_HAS_PASSTHROUGH)\n"
"			, bsdf->hitPoint.passThroughEvent\n"
"#endif\n"
"			);\n"
"}\n"
"\n"
"uint BSDF_GetMaterialExteriorVolume(__global BSDF *bsdf\n"
"		MATERIALS_PARAM_DECL) {\n"
"	return Material_GetExteriorVolume(&mats[bsdf->materialIndex], &bsdf->hitPoint\n"
"#if defined(PARAM_HAS_PASSTHROUGH)\n"
"			, bsdf->hitPoint.passThroughEvent\n"
"#endif\n"
"			);\n"
"}\n"
"#endif\n"
; } }