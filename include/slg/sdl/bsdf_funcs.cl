#line 2 "bsdf_funcs.cl"

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

void BSDF_Init(
		__global BSDF *bsdf,
		//const bool fromL,
		__global Mesh *meshDescs,
		__global uint *meshMats,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global uint *meshTriLightDefsOffset,
#endif
		__global Point *vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
		__global Vector *vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
		__global UV *vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
		__global Spectrum *vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
		__global float *vertAlphas,
#endif
		__global Triangle *triangles,
#if !defined(RENDER_ENGINE_BIASPATHOCL)
		__global
#endif
		Ray *ray,
#if !defined(RENDER_ENGINE_BIASPATHOCL)
		__global
#endif
		RayHit *rayHit
#if defined(PARAM_HAS_PASSTHROUGH)
		, const float u0
#endif
#if defined(PARAM_HAS_VOLUMES)
		, __global PathVolumeInfo *volInfo
#endif
		MATERIALS_PARAM_DECL
		) {
	//bsdf->fromLight = fromL;
#if defined(PARAM_HAS_PASSTHROUGH)
	bsdf->hitPoint.passThroughEvent = u0;
#endif

#if defined(RENDER_ENGINE_BIASPATHOCL)
	const float3 rayOrig = (float3)(ray->o.x, ray->o.y, ray->o.z);
	const float3 rayDir = (float3)(ray->d.x, ray->d.y, ray->d.z);
#else
	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);
#endif
	const float3 hitPointP = rayOrig + rayHit->t * rayDir;
	VSTORE3F(hitPointP, &bsdf->hitPoint.p.x);
	VSTORE3F(-rayDir, &bsdf->hitPoint.fixedDir.x);

	const uint meshIndex = rayHit->meshIndex;
	const uint triangleIndex = rayHit->triangleIndex;

	__global Mesh *meshDesc = &meshDescs[meshIndex];
	__global Point *iVertices = &vertices[meshDesc->vertsOffset];
	__global Triangle *iTriangles = &triangles[meshDesc->trisOffset];

	// Get the material
	const uint matIndex = meshMats[meshIndex];
	bsdf->materialIndex = matIndex;

	//--------------------------------------------------------------------------
	// Get face normal
	//--------------------------------------------------------------------------

	const float b1 = rayHit->b1;
	const float b2 = rayHit->b2;

	// Geometry normal expressed in local coordinates
	float3 geometryN = Mesh_GetGeometryNormal(iVertices, iTriangles, triangleIndex);
	// Transform to global coordinates
	geometryN = normalize(Transform_InvApplyNormal(&meshDesc->trans, geometryN));
	// Store the geometry normal
	VSTORE3F(geometryN, &bsdf->hitPoint.geometryN.x);

	// The shading normal
	float3 shadeN;
#if defined(PARAM_HAS_NORMALS_BUFFER)
	if (meshDesc->normalsOffset != NULL_INDEX) {
		__global Vector *iVertNormals = &vertNormals[meshDesc->normalsOffset];
		// Shading normal expressed in local coordinates
		shadeN = Mesh_InterpolateNormal(iVertNormals, iTriangles, triangleIndex, b1, b2);
		// Transform to global coordinates
		shadeN = normalize(Transform_InvApplyNormal(&meshDesc->trans, shadeN));
	} else
#endif
		shadeN = geometryN;
    VSTORE3F(shadeN, &bsdf->hitPoint.shadeN.x);

	//--------------------------------------------------------------------------
	// Set interior and exterior volumes
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
	bsdf->hitPoint.intoObject = (dot(rayDir, geometryN) < 0.f);

	PathVolumeInfo_SetHitPointVolumes(
			volInfo,
			&bsdf->hitPoint,
			Material_GetInteriorVolume(&mats[matIndex], &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
				, u0
#endif
			),
			Material_GetExteriorVolume(&mats[matIndex], &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
				, u0
#endif
			)
			MATERIALS_PARAM);
#endif

	//--------------------------------------------------------------------------
	// Get UV coordinate
	//--------------------------------------------------------------------------

	float2 hitPointUV;
#if defined(PARAM_HAS_UVS_BUFFER)
	if (meshDesc->uvsOffset != NULL_INDEX) {
		__global UV *iVertUVs = &vertUVs[meshDesc->uvsOffset];
		hitPointUV = Mesh_InterpolateUV(iVertUVs, iTriangles, triangleIndex, b1, b2);
	} else
#endif
		hitPointUV = 0.f;
	VSTORE2F(hitPointUV, &bsdf->hitPoint.uv.u);

	//--------------------------------------------------------------------------
	// Get color value
	//--------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR) || defined(PARAM_ENABLE_TEX_HITPOINTGREY)
	float3 hitPointColor;
#if defined(PARAM_HAS_COLS_BUFFER)
	if (meshDesc->colsOffset != NULL_INDEX) {
		__global Spectrum *iVertCols = &vertCols[meshDesc->colsOffset];
		hitPointColor = Mesh_InterpolateColor(iVertCols, iTriangles, triangleIndex, b1, b2);
	} else
#endif
		hitPointColor = WHITE;
	VSTORE3F(hitPointColor, bsdf->hitPoint.color.c);
#endif

	//--------------------------------------------------------------------------
	// Get alpha value
	//--------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
	float hitPointAlpha;
#if defined(PARAM_HAS_ALPHAS_BUFFER)
	if (meshDesc->colsOffset != NULL_INDEX) {
		__global float *iVertAlphas = &vertAlphas[meshDesc->alphasOffset];
		hitPointAlpha = Mesh_InterpolateAlpha(iVertAlphas, iTriangles, triangleIndex, b1, b2);
	} else
#endif
		hitPointAlpha = 1.f;
	bsdf->hitPoint.alpha = hitPointAlpha;
#endif

	//--------------------------------------------------------------------------

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
	// Check if it is a light source
	bsdf->triangleLightSourceIndex = meshTriLightDefsOffset[meshIndex];
#endif

    //--------------------------------------------------------------------------
	// Build the local reference system
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_UVS_BUFFER)
    if (meshDesc->uvsOffset != NULL_INDEX) {
        // Ok, UV coordinates are available, use them to build the reference
        // system around the shading normal.

        // Compute triangle partial derivatives
        __global Triangle *tri = &iTriangles[triangleIndex];
        const uint vi0 = tri->v[0];
        const uint vi1 = tri->v[1];
        const uint vi2 = tri->v[2];

        __global UV *iVertUVs = &vertUVs[meshDesc->uvsOffset];
        const float2 uv0 = VLOAD2F(&iVertUVs[vi0].u);
        const float2 uv1 = VLOAD2F(&iVertUVs[vi1].u);
        const float2 uv2 = VLOAD2F(&iVertUVs[vi2].u);

        // Compute deltas for triangle partial derivatives
        const float du1 = uv0.s0 - uv2.s0;
        const float du2 = uv1.s0 - uv2.s0;
        const float dv1 = uv0.s1 - uv2.s1;
        const float dv2 = uv1.s1 - uv2.s1;
        const float determinant = du1 * dv2 - dv1 * du2;
        if (determinant == 0.f) {
            // Handle 0 determinant for triangle partial derivative matrix
            Frame_SetFromZ(&bsdf->frame, shadeN);
        } else {
            const float invdet = 1.f / determinant;

            //------------------------------------------------------------------
            // Compute geometryDpdu and geometryDpdv
            //------------------------------------------------------------------

            const float3 p0 = VLOAD3F(&iVertices[vi0].x);
            const float3 p1 = VLOAD3F(&iVertices[vi1].x);
            const float3 p2 = VLOAD3F(&iVertices[vi2].x);
            const float3 dp1 = p0 - p2;
            const float3 dp2 = p1 - p2;

            float3 geometryDpdu = ( dv2 * dp1 - dv1 * dp2) * invdet;
            float3 geometryDpdv = (-du2 * dp1 + du1 * dp2) * invdet;
            // Transform to global coordinates
            geometryDpdu = normalize(Transform_InvApplyNormal(&meshDesc->trans, geometryDpdu));
            geometryDpdv = normalize(Transform_InvApplyNormal(&meshDesc->trans, geometryDpdv));

            //------------------------------------------------------------------
            // Compute shadeDpdu and shadeDpdv
            //------------------------------------------------------------------

            float3 shadeDpdv = normalize(cross(shadeN, geometryDpdu));
            float3 shadeDpdu = cross(shadeDpdv, shadeN);
            shadeDpdv *= (dot(geometryDpdv, shadeDpdv) > 0.f) ? 1.f : -1.f;

            //------------------------------------------------------------------
            // Compute geometryDndu and geometryDndv
            //------------------------------------------------------------------

            float3 geometryDndu, geometryDndv;
#if defined(PARAM_HAS_NORMALS_BUFFER)
            if (meshDesc->normalsOffset != NULL_INDEX) {
                __global Vector *iVertNormals = &vertNormals[meshDesc->normalsOffset];
                // Shading normals expressed in local coordinates
                const float3 n0 = VLOAD3F(&iVertNormals[tri->v[0]].x);
                const float3 n1 = VLOAD3F(&iVertNormals[tri->v[1]].x);
                const float3 n2 = VLOAD3F(&iVertNormals[tri->v[2]].x);
                const float3 dn1 = n0 - n2;
                const float3 dn2 = n1 - n2;

                geometryDndu = ( dv2 * dn1 - dv1 * dn2) * invdet;
                geometryDndv = (-du2 * dn1 + du1 * dn2) * invdet;
                // Transform to global coordinates
                geometryDndu = normalize(Transform_InvApplyNormal(&meshDesc->trans, geometryDndu));
                geometryDndv = normalize(Transform_InvApplyNormal(&meshDesc->trans, geometryDndv));
            } else {
#endif
        		geometryDndu = ZERO;
                geometryDndv = ZERO;
#if defined(PARAM_HAS_NORMALS_BUFFER)
            }
#endif

            //------------------------------------------------------------------
            // Apply bump or normal mapping
            //------------------------------------------------------------------

#if defined(PARAM_HAS_BUMPMAPS)
            Material_Bump(&mats[matIndex],
                    &bsdf->hitPoint, shadeDpdu, shadeDpdv,
                    geometryDndu, geometryDndu, 1.f
                    MATERIALS_PARAM);
            // Re-read the shadeN modified by Material_Bump()
            shadeN = VLOAD3F(&bsdf->hitPoint.shadeN.x);
#endif

            //------------------------------------------------------------------
            // Build the local reference system
            //------------------------------------------------------------------

            VSTORE3F(shadeDpdu, &bsdf->frame.X.x);
            VSTORE3F(shadeDpdv, &bsdf->frame.Y.x);
            VSTORE3F(shadeN, &bsdf->frame.Z.x);
        }
    } else {
#endif
        Frame_SetFromZ(&bsdf->frame, shadeN);
#if defined(PARAM_HAS_UVS_BUFFER)
    }
#endif

#if defined(PARAM_HAS_VOLUMES)
	bsdf->isVolume = false;
#endif
}

float3 BSDF_Evaluate(__global BSDF *bsdf,
		const float3 generatedDir, BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
	//const Vector &eyeDir = fromLight ? generatedDir : hitPoint.fixedDir;
	//const Vector &lightDir = fromLight ? hitPoint.fixedDir : generatedDir;
	const float3 eyeDir = VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	const float3 lightDir = generatedDir;
	const float3 geometryN = VLOAD3F(&bsdf->hitPoint.geometryN.x);

	const float dotLightDirNG = dot(lightDir, geometryN);
	const float absDotLightDirNG = fabs(dotLightDirNG);
	const float dotEyeDirNG = dot(eyeDir, geometryN);
	const float absDotEyeDirNG = fabs(dotEyeDirNG);

	if ((absDotLightDirNG < DEFAULT_COS_EPSILON_STATIC) ||
			(absDotEyeDirNG < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	__global Material *mat = &mats[bsdf->materialIndex];
	const float sideTest = dotEyeDirNG * dotLightDirNG;
	const BSDFEvent matEvent = Material_GetEventTypes(mat
			MATERIALS_PARAM);
	if (((sideTest > 0.f) && !(matEvent & REFLECT)) ||
			((sideTest < 0.f) && !(matEvent & TRANSMIT)))
		return BLACK;

	__global Frame *frame = &bsdf->frame;
	const float3 localLightDir = Frame_ToLocal(frame, lightDir);
	const float3 localEyeDir = Frame_ToLocal(frame, eyeDir);
	const float3 result = Material_Evaluate(mat, &bsdf->hitPoint,
			localLightDir, localEyeDir,	event, directPdfW
			MATERIALS_PARAM);

	// Adjoint BSDF
//	if (fromLight) {
//		const float absDotLightDirNS = AbsDot(lightDir, shadeN);
//		const float absDotEyeDirNS = AbsDot(eyeDir, shadeN);
//		return result * ((absDotLightDirNS * absDotEyeDirNG) / (absDotEyeDirNS * absDotLightDirNG));
//	} else
		return result;
}

float3 BSDF_Sample(__global BSDF *bsdf, const float u0, const float u1,
		float3 *sampledDir, float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		MATERIALS_PARAM_DECL) {
	const float3 fixedDir = VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, fixedDir);
	float3 localSampledDir;

	const float3 result = Material_Sample(&mats[bsdf->materialIndex], &bsdf->hitPoint,
			localFixedDir, &localSampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
			bsdf->hitPoint.passThroughEvent,
#endif
			pdfW, cosSampledDir, event,
			requestedEvent
			MATERIALS_PARAM);
	if (Spectrum_IsBlack(result))
		return 0.f;

	*sampledDir = Frame_ToWorld(&bsdf->frame, localSampledDir);

	// Adjoint BSDF
//	if (fromLight) {
//		const float absDotFixedDirNS = fabsf(localFixedDir.z);
//		const float absDotSampledDirNS = fabsf(localSampledDir.z);
//		const float absDotFixedDirNG = AbsDot(fixedDir, geometryN);
//		const float absDotSampledDirNG = AbsDot(*sampledDir, geometryN);
//		return result * ((absDotFixedDirNS * absDotSampledDirNG) / (absDotSampledDirNS * absDotFixedDirNG));
//	} else
		return result;
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
