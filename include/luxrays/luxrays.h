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

#ifndef _LUXRAYS_H
#define	_LUXRAYS_H

#include <iostream>

#include "luxrays/cfg.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

#define __CL_ENABLE_EXCEPTIONS

#if defined(__APPLE__)
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

#endif // LUXRAYS_DISABLE_OPENCL

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/vector_normal.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/ray.h"
#include "luxrays/core/geometry/raybuffer.h"
#include "luxrays/core/geometry/bbox.h"
#include "luxrays/core/geometry/triangle.h"
#include "luxrays/core/pixel/samplebuffer.h"

/*! \namespace luxrays
 *
 * \brief The LuxRays core classes are defined within this namespace.
 */
namespace luxrays {
class Accelerator;
class Context;
class DataSet;
class IntersectionDevice;
class Mesh;
class PixelDevice;
class TriangleMesh;
class VirtualM2OHardwareIntersectionDevice;
class VirtualM2MHardwareIntersectionDevice;
}

#endif	/* _LUXRAYS_H */
