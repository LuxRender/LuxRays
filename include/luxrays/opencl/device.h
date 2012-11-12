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

#ifndef _LUXRAYS_OPENCL_DEVICE_H
#define	_LUXRAYS_OPENCL_DEVICE_H

#include "luxrays/core/device.h"
#include "luxrays/opencl/opencl.h"

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCL devices
//------------------------------------------------------------------------------

class OpenCLIntersectionDevice;

class OpenCLDeviceDescription : public DeviceDescription {
public:
	OpenCLDeviceDescription(cl::Device &device, const size_t devIndex) :
		DeviceDescription(device.getInfo<CL_DEVICE_NAME>().c_str(),
			GetOCLDeviceType(device.getInfo<CL_DEVICE_TYPE>())),
		deviceIndex(devIndex),
		computeUnits(device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>()),
		maxMemory(device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()),
		maxAllocSizeMemory(device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>()),
		usedMemory(0),
		oclDevice(device),
		oclContext(NULL),
		enableOpenGLInterop(false) { }

	~OpenCLDeviceDescription() {
		delete oclContext;
	}

	size_t GetDeviceIndex() const { return deviceIndex; }
	int GetComputeUnits() const { return computeUnits; }
	size_t GetMaxMemory() const { return maxMemory; }
	size_t GetMaxMemoryAllocSize() const { return maxAllocSizeMemory; }
	size_t GetUsedMemory() const { return usedMemory; }
	void AllocMemory(size_t s) const { usedMemory += s; }
	void FreeMemory(size_t s) const { usedMemory -= s; }

	bool HasImageSupport() const { return oclDevice.getInfo<CL_DEVICE_IMAGE_SUPPORT>() != 0 ; }
	size_t GetImage2DMaxWidth() const { return oclDevice.getInfo<CL_DEVICE_IMAGE2D_MAX_WIDTH>(); }
	size_t GetImage2DMaxHeight() const { return oclDevice.getInfo<CL_DEVICE_IMAGE2D_MAX_HEIGHT>(); }

	bool HasOCLContext() const { return (oclContext != NULL); }
	bool HasOGLInterop() const { return enableOpenGLInterop; }
	void EnableOGLInterop() const {
		if (!oclContext || enableOpenGLInterop)
			enableOpenGLInterop = true;
		else
			throw std::runtime_error("It is not possible to enable OpenGL interoperability when the OpenCL context has laready been created");
	}

	cl::Context &GetOCLContext() const;
	cl::Device &GetOCLDevice() const { return oclDevice; }

	bool IsAMDPlatform() const {
		cl::Platform platform = oclDevice.getInfo<CL_DEVICE_PLATFORM>();
		return !strcmp(platform.getInfo<CL_PLATFORM_VENDOR>().c_str(), "Advanced Micro Devices, Inc.");
	}

	friend class Context;
	friend class OpenCLIntersectionDevice;
	friend class OpenCLPixelDevice;
	friend class OpenCLSampleBuffer;

protected:
	static std::string GetDeviceType(const cl_int type);
	static DeviceType GetOCLDeviceType(const cl_device_type type);
	static void AddDeviceDescs(const cl::Platform &oclPlatform, const DeviceType filter,
		std::vector<DeviceDescription *> &descriptions);

	size_t deviceIndex;
	int computeUnits;
	size_t maxMemory, maxAllocSizeMemory;
	mutable size_t usedMemory;

private:
	mutable cl::Device oclDevice;
	mutable cl::Context *oclContext;
	mutable bool enableOpenGLInterop;
};

}

#endif	/* _LUXRAYS_OPENCL_DEVICE_H */

