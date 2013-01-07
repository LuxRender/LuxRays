#line 2 "light_funcs.cl"

/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_INFINITELIGHT)

float3 InfiniteLight_GetRadiance(
	__global InfiniteLight *infiniteLight, const float3 dir
	IMAGEMAPS_PARAM_DECL) {
	const float2 uv = (float2)(
		1.f - SphericalPhi(-dir) * (1.f / (2.f * M_PI_F))+ infiniteLight->shiftU,
		SphericalTheta(-dir) * M_1_PI_F + infiniteLight->shiftV);

	return vload3(0, &infiniteLight->gain.r) * ImageMapInstance_GetColor(
			&infiniteLight->imageMapInstance, uv
			IMAGEMAPS_PARAM);
}

#endif

//------------------------------------------------------------------------------
// TriangleLight
//------------------------------------------------------------------------------

float3 TriangleLight_Illuminate(__global TriangleLight *triLight,
		__global Material *mats, __global Texture *texs,
		const float3 p, const float u0, const float u1, const float u2,
		float3 *dir, float *distance, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float3 p0 = vload3(0, &triLight->v0.x);
	const float3 p1 = vload3(0, &triLight->v1.x);
	const float3 p2 = vload3(0, &triLight->v2.x);
	float b0, b1, b2;
	float3 samplePoint = Triangle_Sample(
			p0, p1, p2,
			u0, u1,
			&b0, &b1, &b2);
		
	const float3 sampleN = Triangle_GetGeometryNormal(p0, p1, p2); // Light sources are supposed to be flat

	*dir = samplePoint - p;
	const float distanceSquared = dot(*dir, *dir);;
	*distance = sqrt(distanceSquared);
	*dir /= (*distance);

	const float cosAtLight = dot(sampleN, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*directPdfW = triLight->invArea * distanceSquared / cosAtLight;

	const float2 uv0 = vload2(0, &triLight->uv0.u);
	const float2 uv1 = vload2(0, &triLight->uv1.u);
	const float2 uv2 = vload2(0, &triLight->uv2.u);
	const float2 triUV = Triangle_InterpolateUV(uv0, uv1, uv2, b0, b1, b2);

	return Material_GetEmittedRadiance(&mats[triLight->materialIndex], texs, triUV
			IMAGEMAPS_PARAM);
}

float3 TriangleLight_GetRadiance(__global TriangleLight *triLight,
		__global Material *mats, __global Texture *texs, 
		const float3 dir, const float3 hitPointNormal,
		const float2 triUV, float *directPdfA
		IMAGEMAPS_PARAM_DECL) {
	const float cosOutLight = dot(hitPointNormal, dir);
	if (cosOutLight <= 0.f)
		return BLACK;

	if (directPdfA)
		*directPdfA = triLight->invArea;

	return Material_GetEmittedRadiance(&mats[triLight->materialIndex], texs, triUV
			IMAGEMAPS_PARAM);
}
