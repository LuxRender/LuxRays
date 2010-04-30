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
#include <algorithm>

#include "luxrays/kernels/kernels.h"
#include "luxrays/core/pixeldevice.h"
#include "luxrays/core/context.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL PixelDevice
//------------------------------------------------------------------------------

size_t OpenCLPixelDevice::SampleBufferSize = OPENCL_SAMPLEBUFFER_SIZE;

OpenCLPixelDevice::OpenCLPixelDevice(const Context *context, OpenCLDeviceDescription *desc,
		const unsigned int index) :
		PixelDevice(context, DEVICE_TYPE_OPENCL, index) {
	deviceDesc  = desc;
	deviceName = (desc->GetName() +"Pixel").c_str();

	sampleFrameBuffer = NULL;
	frameBuffer = NULL;
	sampleFrameBuff = NULL;
	frameBuff = NULL;

	// Initialize Gaussian2x2_filterTable
	const float alpha = 2.f;
	const float expX = expf(-alpha * Gaussian2x2_xWidth * Gaussian2x2_xWidth);
	const float expY = expf(-alpha * Gaussian2x2_yWidth * Gaussian2x2_yWidth);

	float *ftp2x2 = Gaussian2x2_filterTable;
	for (u_int y = 0; y < FilterTableSize; ++y) {
		const float fy = (static_cast<float>(y) + .5f) * Gaussian2x2_yWidth / FilterTableSize;
		for (u_int x = 0; x < FilterTableSize; ++x) {
			const float fx = (static_cast<float>(x) + .5f) * Gaussian2x2_xWidth / FilterTableSize;
			*ftp2x2++ = Max<float>(0.f, expf(-alpha * fx * fx) - expX) *
					Max<float>(0.f, expf(-alpha * fy * fy) - expY);
		}
	}

	// Compile kernels
	cl::Context &oclContext = deviceDesc->GetOCLContext();
	cl::Device &oclDevice = deviceDesc->GetOCLDevice();

	// Allocate the queue for this device
	oclQueue = new cl::CommandQueue(oclContext, oclDevice, CL_QUEUE_PROFILING_ENABLE);

	//--------------------------------------------------------------------------
	// PixelReset kernel
	//--------------------------------------------------------------------------

	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_ClearFB, "PixelClearFB", &clearFBKernel);
	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_ClearSampleFB, "PixelClearSampleFB", &clearSampleFBKernel);
	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_AddSampleBuffer, "PixelAddSampleBuffer", &addSampleBufferKernel);
	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_AddSampleBufferPreview, "PixelAddSampleBufferPreview", &addSampleBufferPreviewKernel);
	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_AddSampleBufferGaussian2x2, "PixelAddSampleBufferGaussian2x2", &addSampleBufferGaussian2x2Kernel);
	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_UpdateFrameBuffer, "PixelUpdateFrameBuffer", &updateFrameBufferKernel);

	//--------------------------------------------------------------------------
	// Allocate OpenCL SampleBuffers
	//--------------------------------------------------------------------------

	sampleBufferBuff.resize(0);
	sampleBufferBuffEvent.resize(0);
	sampleBuffers.resize(0);
	sampleBuffersUsed.resize(0);

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " GammaTable buffer size: " << (sizeof(float) * GammaTableSize / 1024) << "Kbytes");
	gammaTableBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY,
			sizeof(float) * GammaTableSize);
	deviceDesc->usedMemory += gammaTableBuff->getInfo<CL_MEM_SIZE>();
	SetGamma();

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " FilterTable buffer size: " << (sizeof(float) * FilterTableSize  * FilterTableSize / 1024) << "Kbytes");
	filterTableBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY,
			sizeof(float) * FilterTableSize * FilterTableSize);
	deviceDesc->usedMemory += filterTableBuff->getInfo<CL_MEM_SIZE>();
	oclQueue->enqueueWriteBuffer(
			*filterTableBuff,
			CL_FALSE,
			0,
			sizeof(float) * FilterTableSize * FilterTableSize,
			Gaussian2x2_filterTable);
}

OpenCLPixelDevice::~OpenCLPixelDevice() {
	if (started)
		PixelDevice::Stop();

	delete sampleFrameBuffer;
	delete frameBuffer;

	for (size_t i = 0; i < sampleBufferBuff.size(); ++i) {
		deviceDesc->usedMemory += sampleBufferBuff[i]->getInfo<CL_MEM_SIZE>();
		delete sampleBufferBuff[i];
		delete sampleBuffers[i];
	}

	if (sampleFrameBuff) {
		deviceDesc->usedMemory -= sampleFrameBuff->getInfo<CL_MEM_SIZE>();
		delete sampleFrameBuff;
	}

	if (frameBuff) {
		deviceDesc->usedMemory -= frameBuff->getInfo<CL_MEM_SIZE>();
		delete frameBuff;
	}

	deviceDesc->usedMemory -= filterTableBuff->getInfo<CL_MEM_SIZE>();
	delete filterTableBuff;

	deviceDesc->usedMemory -= gammaTableBuff->getInfo<CL_MEM_SIZE>();
	delete gammaTableBuff;

	delete clearFBKernel;
	delete clearSampleFBKernel;
	delete addSampleBufferKernel;
	delete addSampleBufferPreviewKernel;
	delete addSampleBufferGaussian2x2Kernel;
	delete updateFrameBufferKernel;
	delete oclQueue;
}

void OpenCLPixelDevice::CompileKernel(cl::Context &ctx, cl::Device &device, const std::string &src,
		const char *kernelName, cl::Kernel **kernel) {
	// Compile sources
	cl::Program::Sources source(1, std::make_pair(src.c_str(), src.length()));
	cl::Program program = cl::Program(ctx, source);
	try {
		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(device);
		program.build(buildDevice, "-I.");
	} catch (cl::Error err) {
		cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] " << kernelName << " compilation error:\n" << strError.c_str());

		throw err;
	}

	*kernel = new cl::Kernel(program, kernelName);
}

void OpenCLPixelDevice::AllocateSampleBuffers(const unsigned int count) {
	// Free existing buffers
	for (size_t i = 0; i < sampleBufferBuff.size(); ++i) {
		deviceDesc->usedMemory += sampleBufferBuff[i]->getInfo<CL_MEM_SIZE>();
		delete sampleBufferBuff[i];
		delete sampleBuffers[i];
	}

	// Allocate new one
	sampleBufferBuff.resize(count);
	sampleBufferBuffEvent.resize(count);
	sampleBuffers.resize(count);
	sampleBuffersUsed.resize(count);
	std::fill(sampleBuffersUsed.begin(), sampleBuffersUsed.end(), false);

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " SampleBuffer buffer size: " << (sizeof(SampleBufferElem) * SampleBufferSize / 1024) << "Kbytes (*" << count <<")");

	cl::Context &oclContext = deviceDesc->GetOCLContext();
	for (size_t i = 0; i < sampleBufferBuff.size(); ++i) {
		sampleBufferBuff[i] = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY,
				sizeof(SampleBufferElem) * SampleBufferSize);
		deviceDesc->usedMemory += sampleBufferBuff[i]->getInfo<CL_MEM_SIZE>();

		sampleBuffers[i] = new SampleBuffer(SampleBufferSize);
	}
}

void OpenCLPixelDevice::Init(const unsigned int w, const unsigned int h) {
	PixelDevice::Init(w, h);

	//--------------------------------------------------------------------------
	// Free old buffers
	//--------------------------------------------------------------------------

	delete sampleFrameBuffer;
	delete frameBuffer;

	if (sampleFrameBuff) {
		deviceDesc->usedMemory -= sampleFrameBuff->getInfo<CL_MEM_SIZE>();
		delete sampleFrameBuff;
	}

	if (frameBuff) {
		deviceDesc->usedMemory -= frameBuff->getInfo<CL_MEM_SIZE>();
		delete frameBuff;
	}

	//--------------------------------------------------------------------------
	// Allocate new buffers
	//--------------------------------------------------------------------------

	sampleFrameBuffer = new SampleFrameBuffer(width, height);
	frameBuffer = new FrameBuffer(width, height);

	cl::Context &oclContext = deviceDesc->GetOCLContext();

	// Allocate OpenCL buffers
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " SampleFrameBuffer buffer size: " << (sizeof(SamplePixel) * width * height / 1024) << "Kbytes");
	sampleFrameBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(SamplePixel) * width * height);
	deviceDesc->usedMemory += sampleFrameBuff->getInfo<CL_MEM_SIZE>();

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " FrameBuffer buffer size: " << (sizeof(Pixel) * width * height / 1024) << "Kbytes");
	frameBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(Pixel) * width * height);
	deviceDesc->usedMemory += frameBuff->getInfo<CL_MEM_SIZE>();

	ClearSampleFrameBuffer();
	ClearFrameBuffer();
}

void OpenCLPixelDevice::ClearSampleFrameBuffer() {
	clearSampleFBKernel->setArg(0, *sampleFrameBuff);
	oclQueue->enqueueNDRangeKernel(*clearSampleFBKernel, cl::NullRange,
			cl::NDRange(width, height), cl::NDRange(16, 16));
}

void OpenCLPixelDevice::ClearFrameBuffer() {
	clearFBKernel->setArg(0, *frameBuff);
	oclQueue->enqueueNDRangeKernel(*clearFBKernel, cl::NullRange,
			cl::NDRange(width, height), cl::NDRange(16, 16));
}

void OpenCLPixelDevice::SetGamma(const float gamma) {
	float x = 0.f;
	const float dx = 1.f / GammaTableSize;
	for (unsigned int i = 0; i < GammaTableSize; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / gamma);

	oclQueue->enqueueWriteBuffer(
			*gammaTableBuff,
			CL_FALSE,
			0,
			sizeof(float) * GammaTableSize,
			gammaTable);
}

void OpenCLPixelDevice::Start() {
	boost::mutex::scoped_lock lock(splatMutex);

	PixelDevice::Start();
}

void OpenCLPixelDevice::Interrupt() {
	boost::mutex::scoped_lock lock(splatMutex);
	assert (started);
}

void OpenCLPixelDevice::Stop() {
	boost::mutex::scoped_lock lock(splatMutex);

	PixelDevice::Stop();
}

SampleBuffer *OpenCLPixelDevice::GetFreeSampleBuffer() {
	boost::mutex::scoped_lock lock(splatMutex);

	// Look for a free buffer
	for (size_t i = 0; i < sampleBufferBuff.size(); ++i) {
		if (sampleBuffersUsed[i])
			continue;

		if (!(sampleBufferBuffEvent[i]())) {
			// Ok, found new buffer
			sampleBuffersUsed[i] = true;
			sampleBuffers[i]->Reset();
			return sampleBuffers[i];
		}

		if (sampleBufferBuffEvent[i].getInfo<CL_EVENT_COMMAND_EXECUTION_STATUS>() == CL_COMPLETE) {
			// Ok, found a completed buffer

			// Collect statistics
			statsTotalSampleTime += (sampleBufferBuffEvent[i].getProfilingInfo<CL_PROFILING_COMMAND_END>() -
					sampleBufferBuffEvent[i].getProfilingInfo<CL_PROFILING_COMMAND_START>()) * 10e-9;
			statsTotalSamplesCount += SampleBufferSize;

			sampleBuffersUsed[i] = true;
			sampleBuffers[i]->Reset();
			return sampleBuffers[i];
		}
	}

	// Huston, we have a problem...
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Unable to find a free SampleBuffer");

	// This should never happen, just wait for one
	for (size_t i = 0; i < sampleBufferBuff.size(); ++i) {
		if (!sampleBuffersUsed[i] && (sampleBufferBuffEvent[i].getInfo<CL_EVENT_COMMAND_EXECUTION_STATUS>() != CL_COMPLETE)) {
			sampleBufferBuffEvent[i].wait();
			sampleBuffersUsed[i] = true;
			sampleBuffers[i]->Reset();
			return sampleBuffers[i];
		}
	}

	throw std::runtime_error("Internal error in OpenCLPixelDevice::GetFreeSampleBuffer()");
}

void OpenCLPixelDevice::FreeSampleBuffer(const SampleBuffer *sampleBuffer) {
	boost::mutex::scoped_lock lock(splatMutex);

	for (size_t i = 0; i < sampleBuffers.size(); ++i) {
		if (sampleBuffers[i] == sampleBuffer) {
			sampleBuffersUsed[i] = false;
			return;
		}
	}

	throw std::runtime_error("Internal error: unable to find the SampleBuffer in OpenCLPixelDevice::FreeSampleBuffer()");
}

void OpenCLPixelDevice::AddSampleBuffer(const FilterType type, const SampleBuffer *sampleBuffer) {
	boost::mutex::scoped_lock lock(splatMutex);
	assert (started);

	// Look for the index of the SampleBuffer
	size_t index = sampleBuffers.size();
	for (size_t i = 0; i < sampleBuffers.size(); ++i) {
		if (sampleBuffer == sampleBuffers[i]) {
			// Ok, found
			index = i;
			break;
		}
	}

	if (index == sampleBuffers.size())
		throw std::runtime_error("Internal error: unable to find the SampleBuffer in OpenCLPixelDevice::AddSampleBuffer()");

	// Download the buffer to the GPU
	assert (sampleBuffer->GetSampleCount() < SampleBufferSize);
	oclQueue->enqueueWriteBuffer(
			*(sampleBufferBuff[index]),
			CL_FALSE,
			0,
			sizeof(SampleBufferElem) * sampleBuffer->GetSampleCount(),
			sampleBuffer->GetSampleBuffer());

	// Run the kernel
	sampleBufferBuffEvent[index] = cl::Event();
	switch (type) {
		case FILTER_GAUSSIAN: {
			addSampleBufferGaussian2x2Kernel->setArg(0, width);
			addSampleBufferGaussian2x2Kernel->setArg(1, height);
			addSampleBufferGaussian2x2Kernel->setArg(2, *sampleFrameBuff);
			addSampleBufferGaussian2x2Kernel->setArg(3, (unsigned int)sampleBuffer->GetSampleCount());
			addSampleBufferGaussian2x2Kernel->setArg(4, *(sampleBufferBuff[index]));
			addSampleBufferGaussian2x2Kernel->setArg(5, FilterTableSize);
			addSampleBufferGaussian2x2Kernel->setArg(6, *filterTableBuff);
			addSampleBufferGaussian2x2Kernel->setArg(7, 16 * 256 * sizeof(cl_float), NULL);
			addSampleBufferGaussian2x2Kernel->setArg(8, 16 * 256 * sizeof(cl_float), NULL);

			oclQueue->enqueueNDRangeKernel(*addSampleBufferGaussian2x2Kernel, cl::NullRange,
				cl::NDRange(sampleBuffer->GetSize()), cl::NDRange(256),
				NULL, &(sampleBufferBuffEvent[index]));
			break;
		}
		case FILTER_PREVIEW: {
			addSampleBufferPreviewKernel->setArg(0, width);
			addSampleBufferPreviewKernel->setArg(1, height);
			addSampleBufferPreviewKernel->setArg(2, *sampleFrameBuff);
			addSampleBufferPreviewKernel->setArg(3, (unsigned int)sampleBuffer->GetSampleCount());
			addSampleBufferPreviewKernel->setArg(4, *(sampleBufferBuff[index]));

			oclQueue->enqueueNDRangeKernel(*addSampleBufferPreviewKernel, cl::NullRange,
				cl::NDRange(sampleBuffer->GetSize()), cl::NDRange(256),
				NULL, &(sampleBufferBuffEvent[index]));
			break;
		}
		case FILTER_NONE: {
			addSampleBufferKernel->setArg(0, width);
			addSampleBufferKernel->setArg(1, height);
			addSampleBufferKernel->setArg(2, *sampleFrameBuff);
			addSampleBufferKernel->setArg(3, (unsigned int)sampleBuffer->GetSampleCount());
			addSampleBufferKernel->setArg(4, *(sampleBufferBuff[index]));

			oclQueue->enqueueNDRangeKernel(*addSampleBufferKernel, cl::NullRange,
				cl::NDRange(sampleBuffer->GetSize()), cl::NDRange(256),
				NULL, &(sampleBufferBuffEvent[index]));
			break;
		}
		default:
			assert (false);
			break;
	}

	sampleBuffersUsed[index] = false;
}

void OpenCLPixelDevice::UpdateFrameBuffer() {
	cl::Event event;

	{
		boost::mutex::scoped_lock lock(splatMutex);

		// Run the kernel
		updateFrameBufferKernel->setArg(0, width);
		updateFrameBufferKernel->setArg(1, height);
		updateFrameBufferKernel->setArg(2, *sampleFrameBuff);
		updateFrameBufferKernel->setArg(3, *frameBuff);
		updateFrameBufferKernel->setArg(4, GammaTableSize);
		updateFrameBufferKernel->setArg(5, *gammaTableBuff);

		oclQueue->enqueueNDRangeKernel(*updateFrameBufferKernel, cl::NullRange,
				cl::NDRange(width, height), cl::NDRange(16, 16));

		oclQueue->enqueueReadBuffer(
				*frameBuff,
				CL_FALSE,
				0,
				sizeof(Pixel) * width * height,
				frameBuffer->GetPixels(),
				NULL,
				&event);
	}

	event.wait();
}

void OpenCLPixelDevice::Merge(const SampleFrameBuffer *sfb) {
	throw std::runtime_error("Internal error: OpenCLPixelDevice::Merge() not yet implemented");
}

const SampleFrameBuffer *OpenCLPixelDevice::GetSampleFrameBuffer() const {
	return sampleFrameBuffer;
}
