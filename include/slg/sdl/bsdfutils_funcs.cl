#line 2 "bsdfutils_funcs.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

BSDFEvent BSDF_GetEventTypes(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetEventTypes(&mats[bsdf->materialIndex]
			MATERIALS_PARAM);
}

bool BSDF_IsDelta(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_IsDelta(&mats[bsdf->materialIndex]
			MATERIALS_PARAM);
}

uint BSDF_GetMaterialID(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return mats[bsdf->materialIndex].matID;
}

uint BSDF_GetLightID(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return mats[bsdf->materialIndex].lightID;
}

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
bool BSDF_IsLightSource(__global BSDF *bsdf) {
	return (bsdf->triangleLightSourceIndex != NULL_INDEX);
}

float3 BSDF_GetEmittedRadiance(__global BSDF *bsdf, float *directPdfA
		LIGHTS_PARAM_DECL) {
	const uint triangleLightSourceIndex = bsdf->triangleLightSourceIndex;
	if (triangleLightSourceIndex == NULL_INDEX)
		return BLACK;
	else
		return IntersectableLight_GetRadiance(&lights[triangleLightSourceIndex],
				&bsdf->hitPoint, directPdfA
				LIGHTS_PARAM);
}
#endif

#if defined(PARAM_HAS_PASSTHROUGH)
float3 BSDF_GetPassThroughTransparency(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, VLOAD3F(&bsdf->hitPoint.fixedDir.x));

	return Material_GetPassThroughTransparency(&mats[bsdf->materialIndex],
			&bsdf->hitPoint, localFixedDir, bsdf->hitPoint.passThroughEvent
			MATERIALS_PARAM);
}
#endif

#if defined(PARAM_HAS_VOLUMES)
uint BSDF_GetMaterialInteriorVolume(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetInteriorVolume(&mats[bsdf->materialIndex], &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, bsdf->hitPoint.passThroughEvent
#endif
			);
}

uint BSDF_GetMaterialExteriorVolume(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetExteriorVolume(&mats[bsdf->materialIndex], &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, bsdf->hitPoint.passThroughEvent
#endif
			);
}
#endif