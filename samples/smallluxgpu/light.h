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

#ifndef _LIGHT_H
#define	_LIGHT_H

#include "smalllux.h"
#include "material.h"
#include "texmap.h"
#include "mc.h"
#include "spd.h"

#include "luxrays/luxrays.h"
#include "luxrays/utils/core/exttrianglemesh.h"

class LightSource {
public:
	virtual ~LightSource() { }

	virtual Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const = 0;
};

//------------------------------------------------------------------------------
// InfiniteLight implementations
//------------------------------------------------------------------------------

class InfiniteLight : public LightSource {
public:
	InfiniteLight(TexMapInstance *tx);
	virtual ~InfiniteLight() { }

	virtual void SetGain(const Spectrum &g) {
		gain = g;
	}

	void SetShift(const float su, const float sv) {
		shiftU = su;
		shiftV = sv;
	}

	virtual void Preprocess() { }

	virtual Spectrum Le(const Vector &dir) const;

	virtual Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;

protected:
	TexMapInstance *tex;
	float shiftU, shiftV;
	Spectrum gain;
};

class InfiniteLightBF : public InfiniteLight {
public:
	InfiniteLightBF(TexMapInstance *tx) : InfiniteLight(tx) { }

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
		*pdf = 0;
		return Spectrum();
	}
};

class InfiniteLightPortal : public InfiniteLight {
public:
	InfiniteLightPortal(Context *ctx, TexMapInstance *tx, const string &portalFileName);
	~InfiniteLightPortal();

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;

private:
	ExtTriangleMesh *portals;
	vector<float> portalAreas;
};

class InfiniteLightIS : public InfiniteLight {
public:
	InfiniteLightIS(TexMapInstance *tx);
	~InfiniteLightIS() { delete uvDistrib; }

	void Preprocess();

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;

private:
	Distribution2D *uvDistrib;
};

//------------------------------------------------------------------------------
// Sunsky implementation
//------------------------------------------------------------------------------

class SkyLight : public InfiniteLight {
public:
	SkyLight(float turbidity, const Vector &sundir);
	virtual ~SkyLight() { }

	virtual Spectrum Le(const Vector &dir) const;
	void GetSkySpectralRadiance(const float theta, const float phi, Spectrum * const spect) const;

protected:
	Vector sundir;
	float turbidity;
	float thetaS;
	float phiS;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
};

class SunLight : public LightSource {
public:
	SunLight(float turbidity, float relSize, const Vector &sundir);
	virtual ~SunLight() { }

	Spectrum Le(const Vector &dir) const;

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;

	void SetGain(const Spectrum &g);

protected:
	Vector sundir;
	Spectrum gain;
	float turbidity;
	// XY Vectors for cone sampling
	Vector x, y;
	float thetaS, phiS, V;
	float cosThetaMax, sin2ThetaMax;
	Spectrum suncolor;
};

//------------------------------------------------------------------------------
// TriangleLight implementation
//------------------------------------------------------------------------------

class TriangleLight : public LightSource, public LightMaterial {
public:
	TriangleLight() { }
	TriangleLight(const AreaLightMaterial *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const vector<ExtTriangleMesh *> &objs);

	const Material *GetMaterial() const { return lightMaterial; }

	Spectrum Le(const vector<ExtTriangleMesh *> &objs, const Vector &wo) const;

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;

private:
	const AreaLightMaterial *lightMaterial;
	unsigned int meshIndex, triIndex;
	float area;

};

#endif	/* _LIGHT_H */
