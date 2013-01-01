#line 2 "bsdf_types.cl"

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

typedef enum {
	NONE     = 0,
	DIFFUSE  = 1,
	GLOSSY   = 2,
	SPECULAR = 4,
	REFLECT  = 8,
	TRANSMIT = 16
} BSDFEventType;

typedef int BSDFEvent;

typedef struct {
	// The incoming direction. It is the eyeDir when fromLight = false and
	// lightDir when fromLight = true
	Vector fixedDir;
	Point hitPoint;
	UV hitPointUV;
	Normal geometryN;
	Normal shadeN;

	float passThroughEvent;

#if defined(LUXRAYS_OPENCL_KERNEL)
	__global
#endif
	Triangle *triangles;
	unsigned int triIndex;

#if defined(LUXRAYS_OPENCL_KERNEL)
	__global
#endif
	Material *material;
	//__global TriangleLightSource *lightSource;

	Frame frame;

	// This will be used for BiDir
	//bool fromLight;
} BSDF;
