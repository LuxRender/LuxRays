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

#include <cstdio>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/context.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// Native thread IntersectionDevice
//------------------------------------------------------------------------------

size_t NativeThreadIntersectionDevice::RayBufferSize = 512;

NativeThreadIntersectionDevice::NativeThreadIntersectionDevice(const Context *context,
		const size_t threadIndex, const size_t devIndex) :
			IntersectionDevice(context, DEVICE_TYPE_NATIVE_THREAD, devIndex) {
	char buf[64];
	sprintf(buf, "NativeIntersectThread-%03d", (int)threadIndex);
	deviceName = std::string(buf);
}

NativeThreadIntersectionDevice::~NativeThreadIntersectionDevice() {
	if (started)
		Stop();
}

void NativeThreadIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);
}

void NativeThreadIntersectionDevice::Start() {
	IntersectionDevice::Start();
}

void NativeThreadIntersectionDevice::Interrupt() {
	assert (started);
}

void NativeThreadIntersectionDevice::Stop() {
	IntersectionDevice::Stop();

	doneRayBufferQueue.Clear();
}

RayBuffer *NativeThreadIntersectionDevice::NewRayBuffer() {
	return NewRayBuffer(RayBufferSize);
}

RayBuffer *NativeThreadIntersectionDevice::NewRayBuffer(const size_t size) {
	return new RayBuffer(size);
}

void NativeThreadIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer) {
	Intersect(rayBuffer);

	doneRayBufferQueue.Push(rayBuffer);
}

void NativeThreadIntersectionDevice::Intersect(RayBuffer *rayBuffer) {
	assert (started);

	const double t1 = WallClockTime();

	// Trace rays
	const Ray *rb = rayBuffer->GetRayBuffer();
	RayHit *hb = rayBuffer->GetHitBuffer();
	const size_t rayCount = rayBuffer->GetRayCount();
	for (unsigned int i = 0; i < rayCount; ++i) {
		hb[i].SetMiss();
		dataSet->Intersect(&rb[i], &hb[i]);
	}

	const double t2 = WallClockTime();

	statsTotalRayCount += rayCount;
	statsDeviceTotalTime += t2 - t1;
}

RayBuffer *NativeThreadIntersectionDevice::PopRayBuffer() {
	assert (started);

	return doneRayBufferQueue.Pop();
}
