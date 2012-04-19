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

#ifndef _OCLDATATYPES_H
#define	_OCLDATATYPES_H

#include "smalllux.h"

namespace PathOCL {

typedef struct {
	unsigned int s1, s2, s3;
} Seed;

typedef struct {
	unsigned int state;
	unsigned int depth;
	//unsigned int diffuseVertexCount;

	Spectrum throughput;
} PathState;

typedef struct {
	unsigned int state;
	unsigned int depth;
	//unsigned int diffuseVertexCount;

	Spectrum throughput;

	float bouncePdf;
	int specularBounce;

	Ray nextPathRay;
	Spectrum nextThroughput;
	Spectrum lightRadiance;
} PathStateDL;

typedef struct {
	Spectrum c;
	float count;
} Pixel;

typedef struct {
	float alpha;
} AlphaPixel;

typedef struct {
	unsigned int sampleCount;
} GPUTaskStats;

//------------------------------------------------------------------------------
// Filters
//------------------------------------------------------------------------------

typedef enum {
	NONE, BOX, GAUSSIAN, MITCHELL
} FilterType;

class Filter {
public:
	Filter(const FilterType t, const float wx, const float wy) :
		type(t), widthX(wx), widthY(wy) { }

	FilterType type;
	float widthX, widthY;
};

class NoneFilter : public Filter {
public:
	NoneFilter() : Filter(NONE, 0.f, 0.f) { }
};

class BoxFilter : public Filter {
public:
	BoxFilter(const float wx, const float wy) :
		Filter(BOX, wx, wy) { }
};

class GaussianFilter : public Filter {
public:
	GaussianFilter(const float wx, const float wy, const float a) :
		Filter(GAUSSIAN, wx, wy), alpha(a) { }

	float alpha;
};

class MitchellFilter : public Filter {
public:
	MitchellFilter(const float wx, const float wy, const float b, const float c) :
		Filter(MITCHELL, wx, wy), B(b), C(c) { }

	float B, C;
};

//------------------------------------------------------------------------------
// Samplers
//------------------------------------------------------------------------------

typedef enum {
	INLINED_RANDOM, RANDOM, STRATIFIED, METROPOLIS
} SamplerType;

class Sampler {
public:
	Sampler(const SamplerType t) :
		type(t) { }

	SamplerType type;
};

class InlinedRandomSampler : public Sampler {
public:
	InlinedRandomSampler() :
		Sampler(INLINED_RANDOM) { }

	SamplerType type;
};

class RandomSampler : public Sampler {
public:
	RandomSampler() :
		Sampler(RANDOM) { }

	SamplerType type;
};

class StratifiedSampler : public Sampler {
public:
	StratifiedSampler(const unsigned int x, const unsigned int y) :
		Sampler(STRATIFIED), xSamples(x), ySamples(y) { }

	SamplerType type;
	unsigned int xSamples, ySamples;
};


class MetropolisSampler : public Sampler {
public:
	MetropolisSampler(const float rate, const float reject, const float mutationRate) :
		Sampler(METROPOLIS), largeStepRate(rate), imageMutationRate(mutationRate),
		maxConsecutiveReject(reject) { }

	SamplerType type;
	float largeStepRate, imageMutationRate;
	unsigned int maxConsecutiveReject;
};

//------------------------------------------------------------------------------

#define MAT_MATTE 0
#define MAT_AREALIGHT 1
#define MAT_MIRROR 2
#define MAT_GLASS 3
#define MAT_MATTEMIRROR 4
#define MAT_METAL 5
#define MAT_MATTEMETAL 6
#define MAT_ALLOY 7
#define MAT_ARCHGLASS 8
#define MAT_NULL 9

typedef struct {
    float r, g, b;
} MatteParam;

typedef struct {
    float gain_r, gain_g, gain_b;
} AreaLightParam;

typedef struct {
    float r, g, b;
    int specularBounce;
} MirrorParam;

typedef struct {
    float refl_r, refl_g, refl_b;
    float refrct_r, refrct_g, refrct_b;
    float ousideIor, ior;
    float R0;
    int reflectionSpecularBounce, transmitionSpecularBounce;
} GlassParam;

typedef struct {
	MatteParam matte;
	MirrorParam mirror;
	float matteFilter, totFilter, mattePdf, mirrorPdf;
} MatteMirrorParam;

typedef struct {
    float r, g, b;
    float exponent;
    int specularBounce;
} MetalParam;

typedef struct {
	MatteParam matte;
	MetalParam metal;
	float matteFilter, totFilter, mattePdf, metalPdf;
} MatteMetalParam;

typedef struct {
    float diff_r, diff_g, diff_b;
	float refl_r, refl_g, refl_b;
    float exponent;
    float R0;
    int specularBounce;
} AlloyParam;

typedef struct {
    float refl_r, refl_g, refl_b;
    float refrct_r, refrct_g, refrct_b;
	float transFilter, totFilter, reflPdf, transPdf;
	bool reflectionSpecularBounce, transmitionSpecularBounce;
} ArchGlassParam;

typedef struct {
	unsigned int type;
	union {
		MatteParam matte;
        AreaLightParam areaLight;
		MirrorParam mirror;
        GlassParam glass;
		MatteMirrorParam matteMirror;
        MetalParam metal;
        MatteMetalParam matteMetal;
        AlloyParam alloy;
        ArchGlassParam archGlass;
	} param;
} Material;

//------------------------------------------------------------------------------

typedef struct {
	Point v0, v1, v2;
	Normal normal;
	float area;
	float gain_r, gain_g, gain_b;
} TriangleLight;

typedef struct {
	float shiftU, shiftV;
	Spectrum gain;
	unsigned int width, height;
} InfiniteLight;

typedef struct {
	Vector sundir;
	Spectrum gain;
	float turbidity;
	float relSize;
	// XY Vectors for cone sampling
	Vector x, y;
	float cosThetaMax;
	Spectrum suncolor;
} SunLight;

typedef struct {
	Spectrum gain;
	float thetaS;
	float phiS;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
} SkyLight;

//------------------------------------------------------------------------------

typedef struct {
	unsigned int rgbOffset, alphaOffset;
	unsigned int width, height;
} TexMap;

typedef struct {
	unsigned int vertsOffset;
	unsigned int trisOffset;

	float trans[4][4];
	float invTrans[4][4];
} Mesh;

typedef struct {
	float lensRadius;
	float focalDistance;
	float yon, hither;

	float rasterToCameraMatrix[4][4];
	float cameraToWorldMatrix[4][4];
} Camera;

}


#endif	/* _OCLDATATYPES_H */
