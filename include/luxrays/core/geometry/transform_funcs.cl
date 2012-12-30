#line 2 "randomgen_funcs.cl"

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

float3 Transform_Apply(__global Transform *trans, const float3 point) {
	float4 point4 = (float4)(point.x, point.y, point.z, 1.f);

	const float4 row3 = vload4(0, &trans->m.m[3][0]);
	const float iw = 1.f / dot(row3, point4);

	const float4 row0 = vload4(0, &trans->m.m[0][0]);
	const float4 row1 = vload4(0, &trans->m.m[1][0]);
	const float4 row2 = vload4(0, &trans->m.m[2][0]);
	return (float3)(
			iw * dot(row0, point4),
			iw * dot(row1, point4),
			iw * dot(row2, point4),
			);
}