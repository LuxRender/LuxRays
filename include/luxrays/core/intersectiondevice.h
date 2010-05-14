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

#ifndef _LUXRAYS_INTERSECTIONDEVICE_H
#define	_LUXRAYS_INTERSECTIONDEVICE_H

#include "luxrays/luxrays.h"
#include "luxrays/core/device.h"

namespace luxrays {

class IntersectionDevice : public Device {
public:
	const DataSet *GetDataSet() const { return dataSet; }

	virtual RayBuffer *NewRayBuffer() = 0;
	virtual void PushRayBuffer(RayBuffer *rayBuffer) = 0;
	virtual RayBuffer *PopRayBuffer() = 0;
	virtual size_t GetQueueSize() = 0;

	double GetPerformance() const {
		const double statsTotalRayTime = WallClockTime() - statsStartTime;
		return (statsTotalRayTime == 0.0) ?	1.0 : (statsTotalRayCount / statsTotalRayTime);
	}
	virtual double GetLoad() const = 0;

	friend class Context;
	friend class VirtualM2OHardwareIntersectionDevice;
	friend class VirtualM2MHardwareIntersectionDevice;

protected:
	IntersectionDevice(const Context *context, const DeviceType type, const unsigned int index);
	virtual ~IntersectionDevice();

	virtual void SetDataSet(const DataSet *newDataSet);
	virtual void Start();

	const DataSet *dataSet;
	double statsStartTime, statsTotalRayCount, statsDeviceIdleTime, statsDeviceTotalTime;
};

class HardwareIntersectionDevice : public IntersectionDevice {
protected:
	HardwareIntersectionDevice(const Context *context, const DeviceType type, const unsigned int index) :
		IntersectionDevice(context, type, index) { }
	virtual ~HardwareIntersectionDevice() { }

	virtual void SetExternalRayBufferQueue(RayBufferQueue *queue) = 0;

	friend class VirtualM2OHardwareIntersectionDevice;
	friend class VirtualM2MHardwareIntersectionDevice;
};

//------------------------------------------------------------------------------
// Native thread devices
//------------------------------------------------------------------------------

class NativeThreadIntersectionDevice : public IntersectionDevice {
public:
	NativeThreadIntersectionDevice(const Context *context, const unsigned int threadIndex,
			const unsigned int devIndex);
	~NativeThreadIntersectionDevice();

	void SetDataSet(const DataSet *newDataSet);
	void Start();
	void Interrupt();
	void Stop();

	RayBuffer *NewRayBuffer();
	size_t GetQueueSize() { return 0; }
	void PushRayBuffer(RayBuffer *rayBuffer);
	RayBuffer *PopRayBuffer();

	double GetLoad() const {
		return 1.0;
	}

	void Intersect(RayBuffer *rayBuffer);

	static size_t RayBufferSize;

	friend class Context;

private:
	RayBufferSingleQueue doneRayBufferQueue;
};

//------------------------------------------------------------------------------
// OpenCL devices
//------------------------------------------------------------------------------

#define OPENCL_RAYBUFFER_SIZE 65536

#if !defined(LUXRAYS_DISABLE_OPENCL)

class OpenCLIntersectionDevice : public HardwareIntersectionDevice {
public:
	OpenCLIntersectionDevice(const Context *context, OpenCLDeviceDescription *desc,
			const unsigned int index, const unsigned int forceWorkGroupSize);
	~OpenCLIntersectionDevice();

	void SetDataSet(const DataSet *newDataSet);
	void Start();
	void Interrupt();
	void Stop();

	RayBuffer *NewRayBuffer();
	size_t GetQueueSize() { return rayBufferQueue.GetSizeToDo(); }
	void PushRayBuffer(RayBuffer *rayBuffer);
	RayBuffer *PopRayBuffer();

	const OpenCLDeviceDescription *GetDeviceDesc() const { return deviceDesc; }

	double GetLoad() const {
		return (statsDeviceTotalTime == 0.0) ? 0.0 : (1.0 - statsDeviceIdleTime / statsDeviceTotalTime);
	}

	friend class Context;

	static size_t RayBufferSize;

protected:
	void SetExternalRayBufferQueue(RayBufferQueue *queue);

private:
	static void IntersectionThread(OpenCLIntersectionDevice *renderDevice);

	void TraceRayBuffer(RayBuffer *rayBuffer, cl::Event *event);

	OpenCLDeviceDescription *deviceDesc;
	boost::thread *intersectionThread;

	// OpenCL items
	cl::CommandQueue *oclQueue;

	// BVH fields
	cl::Kernel *bvhKernel;
	size_t bvhWorkGroupSize;
	cl::Buffer *vertsBuff;
	cl::Buffer *trisBuff;
	cl::Buffer *bvhBuff;

	// QBVH fields

	// QBVH with normal storage fields
	cl::Kernel *qbvhKernel;
	size_t qbvhWorkGroupSize;
	cl::Buffer *qbvhBuff;
	cl::Buffer *qbvhTrisBuff;

	// QBVH with image storage fields
	cl::Kernel *qbvhImageKernel;
	size_t qbvhImageWorkGroupSize;

	cl::Image2D *qbvhImageBuff;
	cl::Image2D *qbvhTrisImageBuff;

	cl::Buffer *raysBuff;
	cl::Buffer *hitsBuff;

	RayBufferQueueO2O rayBufferQueue;
	RayBufferQueue *externalRayBufferQueue;

	bool reportedPermissionError, qbvhUseImage;
};

#endif

}

#endif	/* _LUXRAYS_INTERSECTIONDEVICE_H */
