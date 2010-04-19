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
		double statsTotalRayTime = WallClockTime() - statsStartTime;
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
	NativeThreadIntersectionDevice(const Context *context, const size_t threadIndex,
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

protected:
	static void AddDevices(std::vector<DeviceDescription *> &descriptions);

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
	OpenCLIntersectionDevice(const Context *context, const cl::Device &device,
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

	OpenCLDeviceType GetOpenCLType() const { return oclType; }

	double GetLoad() const {
		return (statsDeviceTotalTime == 0.0) ? 0.0 : (1.0 - statsDeviceIdleTime / statsDeviceTotalTime);
	}

	friend class Context;

	static size_t RayBufferSize;

protected:
	void SetExternalRayBufferQueue(RayBufferQueue *queue);

	static std::string GetDeviceType(const cl_int type);
	static std::string GetDeviceType(const OpenCLDeviceType type);
	static OpenCLDeviceType GetOCLDeviceType(const cl_int type);
	static void AddDevices(const cl::Platform &oclPlatform, const OpenCLDeviceType filter,
		std::vector<DeviceDescription *> &descriptions);

private:
	static void IntersectionThread(OpenCLIntersectionDevice *renderDevice);

	void TraceRayBuffer(RayBuffer *rayBuffer, cl::Event *event);

	OpenCLDeviceType oclType;
	boost::thread *intersectionThread;

	// OpenCL items
	cl::Context *oclContext;
	cl::CommandQueue *oclQueue;

	// BVH fields
	cl::Kernel *bvhKernel;
	size_t bvhWorkGroupSize;
	cl::Buffer *vertsBuff;
	cl::Buffer *trisBuff;
	cl::Buffer *bvhBuff;

	// QBVH fields
	cl::Kernel *qbvhKernel;
	size_t qbvhWorkGroupSize;
	cl::Buffer *qbvhBuff;
	cl::Buffer *qbvhTrisBuff;

	cl::Buffer *raysBuff;
	cl::Buffer *hitsBuff;

	RayBufferQueueO2O rayBufferQueue;
	RayBufferQueue *externalRayBufferQueue;

	bool reportedPermissionError;
};

#endif

}

#endif	/* _LUXRAYS_INTERSECTIONDEVICE_H */
