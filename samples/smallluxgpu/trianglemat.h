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

#ifndef _TRIANGLEMAT_H
#define	_TRIANGLEMAT_H

#include "smalllux.h"
#include "material.h"

#include "luxrays/luxrays.h"
#include "luxrays/utils/core/exttrianglemesh.h"

class TriangleLight : public LightMaterial {
public:
	TriangleLight() { }

	TriangleLight(const AreaLightMaterial *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const vector<ExtTriangleMesh *> &objs) {
		lightMaterial = mat;
		meshIndex = mshIndex;
		triIndex = triangleIndex;

		const ExtTriangleMesh *mesh = objs[meshIndex];
		area = (mesh->GetTriangles()[triIndex]).Area(mesh->GetVertices());
	}

	const Material *GetMaterial() const { return lightMaterial; }

	Spectrum Le(const vector<ExtTriangleMesh *> &objs, const Vector &wo) const {
		const ExtTriangleMesh *mesh = objs[meshIndex];
		const Triangle &tri = mesh->GetTriangles()[triIndex];
		Normal sampleN = mesh->GetNormal()[tri.v[0]]; // Light sources are supposed to be flat

		if (Dot(sampleN, wo) <= 0.f)
			return Spectrum();

		const Spectrum *colors = mesh->GetColors();
		if (colors)
			return mesh->GetColors()[tri.v[0]] * lightMaterial->GetGain(); // Light sources are supposed to have flat color
		else
			return lightMaterial->GetGain(); // Light sources are supposed to have flat color
	}

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, float *pdf, Ray *shadowRay) const {
		const ExtTriangleMesh *mesh = objs[meshIndex];
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		Point samplePoint;
		float b0, b1, b2;
		tri.Sample(mesh->GetVertices(), u0, u1, &samplePoint, &b0, &b1, &b2);
		const Normal sampleN = mesh->GetNormal()[tri.v[0]]; // Light sources are supposed to be flat

		Vector wi = samplePoint - p;
		const float distanceSquared = wi.LengthSquared();
		const float distance = sqrtf(distanceSquared);
		wi /= distance;

		float SampleNdotMinusWi = Dot(sampleN, -wi);
		float NdotMinusWi = Dot(N, wi);
		if ((SampleNdotMinusWi <= 0.f) || (NdotMinusWi <= 0.f)) {
			*pdf = 0.f;
			return Spectrum();
		}

		*shadowRay = Ray(p, wi, RAY_EPSILON, distance - RAY_EPSILON);
		*pdf = distanceSquared / (SampleNdotMinusWi * area);

		const Spectrum *colors = mesh->GetColors();
		if (colors)
			return mesh->GetColors()[tri.v[0]] * lightMaterial->GetGain(); // Light sources are supposed to have flat color
		else
			return lightMaterial->GetGain(); // Light sources are supposed to have flat color
	}

private:
	const AreaLightMaterial *lightMaterial;
	unsigned int meshIndex, triIndex;
	float area;

};

#endif	/* _TRIANGLEMAT_H */

