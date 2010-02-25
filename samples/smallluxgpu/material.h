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

#ifndef _MATERIAL_H
#define	_MATERIAL_H

#include <vector>

#include "smalllux.h"
#include "luxrays/utils/core/exttrianglemesh.h"

class Material {
public:
	virtual ~Material() { }

	virtual bool IsLightSource() const = 0;
	virtual bool IsLambertian() const = 0;
	virtual bool IsSpecular() const = 0;
};

class LightMaterial : public Material {
public:
	bool IsLightSource() const { return true; }
	bool IsLambertian() const { return false; }
	bool IsSpecular() const { return false; }
};

class AreaLightMaterial : public LightMaterial {
public:
	AreaLightMaterial(const Spectrum col) { gain = col; }

	const Spectrum &GetGain() const { return gain; }

private:
	Spectrum gain;
};

class SurfaceMaterial : public Material {
public:
	bool IsLightSource() const { return false; }

	virtual Spectrum f(const Vector &wi, const Vector &wo, const Normal &N) const = 0;
	virtual Spectrum Sample_f(const Vector &wi, Vector *wo, const Normal &N,
		const Normal &shadeN, const float u0, const float u1,  const float u2,
		float *pdf,	bool &specularBounce) const = 0;
};

class MatteMaterial : public SurfaceMaterial {
public:
	MatteMaterial(const Spectrum &col) {
		Kd = col;
		KdOverPI = Kd * INV_PI;
	}

	bool IsLambertian() const { return true; }
	bool IsSpecular() const { return false; }


	Spectrum f(const Vector &wi, const Vector &wo, const Normal &N) const {
		return KdOverPI;
	}

	Spectrum Sample_f(const Vector &wi, Vector *wo, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, float *pdf, bool &specularBounce) const {
		float r1 = 2.f * M_PI * u0;
		float r2 = u1;
		float r2s = sqrt(r2);
		const Vector w(shadeN);

		Vector u;
		if (fabsf(shadeN.x) > .1f) {
			const Vector a(0.f, 1.f, 0.f);
			u = Cross(a, w);
		} else {
			const Vector a(1.f, 0.f, 0.f);
			u = Cross(a, w);
		}
		u = Normalize(u);

		Vector v = Cross(w, u);

		(*wo) = Normalize(u * (cosf(r1) * r2s) + v * (sinf(r1) * r2s) + w * sqrtf(1.f - r2));

		specularBounce = false;
		*pdf = AbsDot(wi, shadeN) * INV_PI;

		return KdOverPI;
	}

	const Spectrum &GetKd() const { return Kd; }

private:
	Spectrum Kd, KdOverPI;
};

class MirrorMaterial : public SurfaceMaterial {
public:
	MirrorMaterial(const Spectrum &refl) {
		Kr = refl;
	}

	bool IsLambertian() const { return false; }
	bool IsSpecular() const { return true; }

	Spectrum f(const Vector &wi, const Vector &wo, const Normal &N) const {
		throw std::runtime_error("Internal error, called MirrorMaterial::f()");
	}

	Spectrum Sample_f(const Vector &wi, Vector *wo, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, float *pdf, bool &specularBounce) const {
		const Vector dir = -wi;
		(*wo) = dir - (2.f * Dot(shadeN, dir)) * Vector(shadeN);

		specularBounce = true;
		*pdf = 1.f;

		return Kr;
	}

	const Spectrum &GetKr() const { return Kr; }

private:
	Spectrum Kr;
};

class MatteMirrorMaterial : public SurfaceMaterial {
public:
	MatteMirrorMaterial(const Spectrum &col, const Spectrum refl) : matte(col), mirror (refl) {
		matteFilter = matte.GetKd().Filter();
		mirrorFilter = mirror.GetKr().Filter();
		totFilter = matteFilter + mirrorFilter;

		mattePdf = matteFilter / totFilter;
		mirrorPdf = mirrorFilter / totFilter;
	}

	bool IsLambertian() const { return true; }
	bool IsSpecular() const { return true; }

	Spectrum f(const Vector &wi, const Vector &wo, const Normal &N) const {
		return matte.f(wi, wo, N);
	}

	Spectrum Sample_f(const Vector &wi, Vector *wo, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, float *pdf, bool &specularBounce) const {
		const float comp = u2 * totFilter;

		if (comp > matteFilter) {
			const Spectrum f = mirror.Sample_f(wi, wo, N, shadeN, u0, u1, u2, pdf, specularBounce);
			*pdf *= mirrorPdf;

			return f;
		} else {
			const Spectrum f = matte.Sample_f(wi, wo, N, shadeN, u0, u1, u2, pdf, specularBounce);
			*pdf *= mattePdf;

			return f;
		}
	}

private:
	MatteMaterial matte;
	MirrorMaterial mirror;
	float matteFilter, mirrorFilter, totFilter, mattePdf, mirrorPdf;
};

class GlassMaterial : public SurfaceMaterial {
public:
	GlassMaterial(const Spectrum &refl, const Spectrum &refrct, const float iorFact) {
		Krefl = refl;
		Krefrct = refrct;
		ior = iorFact;
	}

	bool IsLambertian() const { return false; }
	bool IsSpecular() const { return true; }

	Spectrum f(const Vector &wi, const Vector &wo, const Normal &N) const {
		throw std::runtime_error("Internal error, called GlassMaterial::f()");
	}

	Spectrum Sample_f(const Vector &wi, Vector *wo, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, float *pdf, bool &specularBounce) const {
		Vector reflDir = -wi;
		reflDir = reflDir - (2.f * Dot(N, reflDir)) * Vector(N);

		// Ray from outside going in ?
		const bool into = (Dot(N, shadeN) > 0);

		const float nc = 1.f;
		const float nt = ior;
		const float nnt = into ? (nc / nt) : (nt / nc);
		const float ddn = Dot(-wi, shadeN);
		const float cos2t = 1.f - nnt * nnt * (1.f - ddn * ddn);

		// Total internal reflection
		if (cos2t < 0.f) {
			(*wo) = reflDir;
			*pdf = 1.f;
			specularBounce = true;

			return Krefl;
		}

		const float kk = (into ? 1.f : -1.f) * (ddn * nnt + sqrt(cos2t));
		const Vector nkk = kk * Vector(N);
		const Vector transDir = Normalize(nnt * (-wi) - nkk);

		const float a = nt - nc;
		const float b = nt + nc;
		const float R0 = a * a / (b * b);
		const float c = 1.f - (into ? -ddn : Dot(transDir, N));

		const float Re = R0 + (1.f - R0) * c * c * c * c * c;
		const float Tr = 1.f - Re;
		const float P = .25f + .5f * Re;
		const float RP = P / Re;
		const float TP = (1.f - P) / Tr;

		if (u0 < P) {
			(*wo) = reflDir;
			*pdf = RP;
			specularBounce = true;

			return Krefl;
		} else {
			(*wo) = transDir;
			*pdf = TP;
			specularBounce = false;

			return Krefrct;
		}
	}

	const Spectrum &GetKrefl() const { return Krefl; }
	const Spectrum &GetKrefrct() const { return Krefrct; }

private:
	Spectrum Krefl, Krefrct;
	float ior;
};

#endif	/* _MATERIAL_H */
