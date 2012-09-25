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

typedef Spectrum Pixel;

__kernel __attribute__((reqd_work_group_size(8, 8, 1))) void PixelClearFB(
	const unsigned int width,
	const unsigned int height,
    __global Pixel *frameBuffer) {
    const unsigned int px = get_global_id(0);
    if(px >= width)
        return;
    const unsigned int py = get_global_id(1);
    if(py >= height)
        return;
	const unsigned int offset = px + py * width;

	__global Pixel *p = &frameBuffer[offset];
	p->r = 0.f;
	p->g = 0.f;
	p->b = 0.f;
}
