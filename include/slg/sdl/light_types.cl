#line 2 "light_types.cl"

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

typedef enum {
	TYPE_IL, TYPE_IL_SKY, TYPE_SUN, TYPE_TRIANGLE, TYPE_POINT, TYPE_MAPPOINT,
	TYPE_SPOT, TYPE_PROJECTION, TYPE_IL_CONSTANT, TYPE_SHARPDISTANT, TYPE_DISTANT,
	TYPE_IL_SKY2,
	LIGHT_SOURCE_TYPE_COUNT
} LightSourceType;

typedef struct {
	TextureMapping2D mapping;
	unsigned int imageMapIndex;
	unsigned int distributionOffset;
} InfiniteLightParam;

typedef struct {
	float absoluteTheta;
	float absolutePhi;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
} SkyLightParam;

typedef struct {
	Vector absoluteSunDir;
	Spectrum aTerm, bTerm, cTerm, dTerm, eTerm, fTerm,
		gTerm, hTerm, iTerm, radianceTerm;
} SkyLight2Param;

typedef struct {
	Vector absoluteDir;
	float turbidity, relSize;
	// XY Vectors for cone sampling
	Vector x, y;
	float cosThetaMax;
	Spectrum color;
} SunLightParam;

typedef struct {
	Vector absolutePos;
	Spectrum emittedFactor;
} PointLightParam;

typedef struct {
	Vector absolutePos, localPos;
	Spectrum emittedFactor;
	float avarage;
	unsigned int imageMapIndex;
} MapPointLightParam;

typedef struct {
	Vector absolutePos;
	Spectrum emittedFactor;
	float cosTotalWidth, cosFalloffStart;
} SpotLightParam;

typedef struct {
	Vector absolutePos, lightNormal;
	Spectrum emittedFactor;
	Matrix4x4 lightProjection;
	float screenX0, screenX1, screenY0, screenY1;
	unsigned int imageMapIndex;
} ProjectionLightParam;

typedef struct {
	Spectrum color;
} ConstantInfiniteLightParam;

typedef struct {
	Spectrum color;
	Vector absoluteLightDir;
} SharpDistantLightParam;

typedef struct {
	Spectrum color;
	Vector absoluteLightDir, x, y;
	float cosThetaMax;
} DistantLightParam;

typedef struct {
	Transform light2World;
	Spectrum gain;

	union {
		SunLightParam sun;
		SkyLightParam sky;
		SkyLight2Param sky2;
		InfiniteLightParam infinite;
		PointLightParam point;
		MapPointLightParam mapPoint;
		SpotLightParam spot;
		ProjectionLightParam projection;
		ConstantInfiniteLightParam constantInfinite;
		SharpDistantLightParam sharpDistant;
		DistantLightParam distant;
	};
} NotIntersecableLightSource;

typedef struct {
	Vector v0, v1, v2;
	UV uv0, uv1, uv2;
	float invArea;

	unsigned int materialIndex;
	unsigned int lightSceneIndex;

	// Used for image map and/or IES map
	float avarage;
	unsigned int imageMapIndex;
} TriangleLightParam;

typedef struct {
	LightSourceType type;
	unsigned int lightSceneIndex;
	unsigned int lightID;
	int samples;
	// Type of indirect paths where a light source is visible with a direct hit. It is
	// an OR of DIFFUSE, GLOSSY and SPECULAR.
	BSDFEvent visibility;
	
	union {
		NotIntersecableLightSource notIntersecable;
		TriangleLightParam triangle;
	};
} LightSource;


//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#if defined(PARAM_HAS_INFINITELIGHT)
#define LIGHTS_PARAM_DECL , __global LightSource *lights, __global uint *envLightIndices, const uint envLightCount, __global uint *meshTriLightDefsOffset, __global float *infiniteLightDistribution, __global float *lightsDistribution MATERIALS_PARAM_DECL
#define LIGHTS_PARAM , lights, envLightIndices, envLightCount, meshTriLightDefsOffset, infiniteLightDistribution, lightsDistribution MATERIALS_PARAM
#elif defined(PARAM_HAS_ENVLIGHTS)
#define LIGHTS_PARAM_DECL , __global LightSource *lights, __global uint *envLightIndices, const uint envLightCount, __global uint *meshTriLightDefsOffset, __global float *lightsDistribution MATERIALS_PARAM_DECL
#define LIGHTS_PARAM , lights, envLightIndices, envLightCount, meshTriLightDefsOffset, lightsDistribution MATERIALS_PARAM
#else
#define LIGHTS_PARAM_DECL , __global LightSource *lights, __global uint *meshTriLightDefsOffset, __global float *lightsDistribution MATERIALS_PARAM_DECL
#define LIGHTS_PARAM , lights, meshTriLightDefsOffset, lightsDistribution MATERIALS_PARAM
#endif

#endif
