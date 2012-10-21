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
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <string>
#include <sstream>
#include <stdexcept>

#include <boost/thread/mutex.hpp>

#include "smalllux.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "pathocl/pathocl.h"
#include "pathocl/kernels/kernels.h"
#include "renderconfig.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/core/pixel/samplebuffer.h"

//------------------------------------------------------------------------------
// PathOCLRenderEngine
//------------------------------------------------------------------------------

PathOCLRenderEngine::PathOCLRenderEngine(RenderConfig *rcfg, NativeFilm *flm, boost::mutex *flmMutex) :
		OCLRenderEngine(rcfg, flm, flmMutex) {
	const Properties &cfg = renderConfig->cfg;
	compiledScene = NULL;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	taskCount = RoundUpPow2(cfg.GetInt("opencl.task.count", 65536));
	SLG_LOG("[PathOCLRenderThread] OpenCL task count: " << taskCount);

	maxPathDepth = cfg.GetInt("path.maxdepth", 5);
	maxDiffusePathVertexCount = cfg.GetInt("path.maxdiffusebounce", 5);
	rrDepth = cfg.GetInt("path.russianroulette.depth", 3);
	rrImportanceCap = cfg.GetFloat("path.russianroulette.cap", 0.125f);
	epsilon = cfg.GetFloat("scene.epsilon", .0001f);

	//--------------------------------------------------------------------------
	// Sampler
	//--------------------------------------------------------------------------

	 const string samplerTypeName = cfg.GetString("path.sampler.type", "INLINED_RANDOM");
	 if (samplerTypeName.compare("INLINED_RANDOM") == 0)
		 sampler = new PathOCL::InlinedRandomSampler();
	 else if (samplerTypeName.compare("RANDOM") == 0)
		 sampler = new PathOCL::RandomSampler();
	 else if (samplerTypeName.compare("STRATIFIED") == 0) {
		 const unsigned int xSamples = cfg.GetInt("path.sampler.xsamples", 3);
		 const unsigned int ySamples = cfg.GetInt("path.sampler.ysamples", 3);

		 sampler = new PathOCL::StratifiedSampler(xSamples, ySamples);
	 } else if (samplerTypeName.compare("METROPOLIS") == 0) {
		 const float rate = cfg.GetFloat("path.sampler.largesteprate", .4f);
		 const float reject = cfg.GetFloat("path.sampler.maxconsecutivereject", 512);
		 const float mutationrate = cfg.GetFloat("path.sampler.imagemutationrate", .1f);

		 sampler = new PathOCL::MetropolisSampler(rate, reject, mutationrate);
	 } else
		throw std::runtime_error("Unknown path.sampler.type");

	//--------------------------------------------------------------------------
	// Filter
	//--------------------------------------------------------------------------

	const string filterType = cfg.GetString("path.filter.type", "NONE");
	const float filterWidthX = cfg.GetFloat("path.filter.width.x", 1.5f);
	const float filterWidthY = cfg.GetFloat("path.filter.width.y", 1.5f);
	if ((filterWidthX <= 0.f) || (filterWidthX > 1.5f))
		throw std::runtime_error("path.filter.width.x must be between 0.0 and 1.5");
	if ((filterWidthY <= 0.f) || (filterWidthY > 1.5f))
		throw std::runtime_error("path.filter.width.y must be between 0.0 and 1.5");

	if (filterType.compare("NONE") == 0)
		filter = new PathOCL::NoneFilter();
	else if (filterType.compare("BOX") == 0)
		filter = new PathOCL::BoxFilter(filterWidthX, filterWidthY);
	else if (filterType.compare("GAUSSIAN") == 0) {
		const float alpha = cfg.GetFloat("path.filter.alpha", 2.f);
		filter = new PathOCL::GaussianFilter(filterWidthX, filterWidthY, alpha);
	} else if (filterType.compare("MITCHELL") == 0) {
		const float B = cfg.GetFloat("path.filter.B", 1.f / 3.f);
		const float C = cfg.GetFloat("path.filter.C", 1.f / 3.f);
		filter = new PathOCL::MitchellFilter(filterWidthX, filterWidthY, B, C);
	} else
		throw std::runtime_error("Unknown path.filter.type");

	usePixelAtomics = (cfg.GetInt("path.pixelatomics.enable", 0) != 0);	

	startTime = 0.0;
	samplesCount = 0;
	convergence = 0.f;

	sampleBuffer = film->GetFreeSampleBuffer();

	const unsigned int seedBase = (unsigned int)(WallClockTime() / 1000.0);

	// Create and start render threads
	const size_t renderThreadCount = oclIntersectionDevices.size();
	SLG_LOG("Starting "<< renderThreadCount << " PathOCL render threads");
	for (size_t i = 0; i < renderThreadCount; ++i) {
		PathOCLRenderThread *t = new PathOCLRenderThread(
				i, seedBase + i * taskCount, i / (float)renderThreadCount,
				oclIntersectionDevices[i], this);
		renderThreads.push_back(t);
	}
}

PathOCLRenderEngine::~PathOCLRenderEngine() {
	if (editMode)
		EndEditLockLess(EditActionList());
	if (started)
		StopLockLess();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];

	film->FreeSampleBuffer(sampleBuffer);

	delete sampler;
	delete filter;
}

void PathOCLRenderEngine::Start() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	compiledScene = new CompiledScene(renderConfig, film);

	OCLRenderEngine::StartLockLess();

	samplesCount = 0;
	elapsedTime = 0.0f;

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();

	startTime = WallClockTime();
	film->ResetConvergenceTest();
	lastConvergenceTestTime = startTime;
	lastConvergenceTestSamplesCount = 0;
}

void PathOCLRenderEngine::Stop() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();

	UpdateFilmLockLess();

	OCLRenderEngine::StopLockLess();

	delete compiledScene;
	compiledScene = NULL;
}

void PathOCLRenderEngine::BeginEdit() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	/*SLG_LOG("[DEBUG] BeginEdit() =================================");
	const double t1 = WallClockTime();*/
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->BeginEdit();

	//const double t2 = WallClockTime();
	OCLRenderEngine::BeginEditLockLess();

	/*const double t3 = WallClockTime();
	SLG_LOG("[DEBUG] T1 = " << int((t2 - t1) * 1000.0) <<
		" T2 = " << int((t3 - t2) * 1000.0));*/
}

void PathOCLRenderEngine::EndEdit(const EditActionList &editActions) {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	/*SLG_LOG("[DEBUG] EndEdit() =================================");
	const double t1 = WallClockTime();*/
	OCLRenderEngine::EndEditLockLess(editActions);

	//const double t2 = WallClockTime();
	compiledScene->Recompile(editActions);

	//const double t3 = WallClockTime();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->EndEdit(editActions);

	/*const double t4 = WallClockTime();
	SLG_LOG("[DEBUG] T1 = " << int((t2 - t1) * 1000.0) <<
		" T2 = " << int((t3 - t2) * 1000.0) <<
		" T3 = " << int((t4 - t3) * 1000.0));*/

	elapsedTime = 0.0f;
	startTime = WallClockTime();
	film->ResetConvergenceTest();
	lastConvergenceTestTime = startTime;
	lastConvergenceTestSamplesCount = 0;
}

void PathOCLRenderEngine::UpdateFilm() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	if (started)
		UpdateFilmLockLess();
}

void PathOCLRenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	elapsedTime = WallClockTime() - startTime;
	const unsigned int imgWidth = film->GetWidth();
	const unsigned int imgHeight = film->GetHeight();

	film->Reset();

	const bool isAlphaChannelEnabled = film->IsAlphaChannelEnabled();

	switch (film->GetFilterType()) {
		case FILTER_GAUSSIAN: {
			SampleBufferElem sbe;

			for (unsigned int y = 0; y < imgHeight; ++y) {
				unsigned int pGPU = 1 + (y + 1) * (imgWidth + 2);

				for (unsigned int x = 0; x < imgWidth; ++x) {
					Spectrum c;
					float alpha = 0.0f;
					float count = 0.f;
					for (size_t i = 0; i < renderThreads.size(); ++i) {
						if (renderThreads[i]->frameBuffer) {
							c += renderThreads[i]->frameBuffer[pGPU].c;
							count += renderThreads[i]->frameBuffer[pGPU].count;
						}

						if (renderThreads[i]->alphaFrameBuffer)
							alpha += renderThreads[i]->alphaFrameBuffer[pGPU].alpha;
					}

					if ((count > 0) && !c.IsNaN()) {
						sbe.screenX = x;
						sbe.screenY = y;
						c /= count;
						sbe.radiance = c;

						film->SplatFiltered(&sbe);

						if (isAlphaChannelEnabled && !isnan(alpha))
							film->SplatFilteredAlpha(x, y, alpha / count);
					}

					++pGPU;
				}
			}
			break;
		}
		case FILTER_NONE: {
			for (unsigned int y = 0; y < imgHeight; ++y) {
				unsigned int pGPU = 1 + (y + 1) * (imgWidth + 2);

				for (unsigned int x = 0; x < imgWidth; ++x) {
					Spectrum c;
					float alpha = 0.0f;
					float count = 0.f;
					for (size_t i = 0; i < renderThreads.size(); ++i) {
						if (renderThreads[i]->frameBuffer) {
							c += renderThreads[i]->frameBuffer[pGPU].c;
							count += renderThreads[i]->frameBuffer[pGPU].count;
						}
						
						if (renderThreads[i]->alphaFrameBuffer)
							alpha += renderThreads[i]->alphaFrameBuffer[pGPU].alpha;
					}

					if ((count > 0) && !c.IsNaN()) {
						film->AddRadiance(x, y, c, count);

						if (isAlphaChannelEnabled && !isnan(alpha))
							film->AddAlpha(x, y, alpha);
					}

					++pGPU;
				}
			}
			break;
		}
		default:
			assert (false);
			break;
	}

	// Update the sample count statistic
	unsigned long long totalCount = 0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		PathOCL::GPUTaskStats *stats = renderThreads[i]->gpuTaskStats;

		for (size_t i = 0; i < taskCount; ++i)
			totalCount += stats[i].sampleCount;
	}

	samplesCount = totalCount;

	const float haltthreshold = renderConfig->cfg.GetFloat("batch.haltthreshold", -1.f);
	if (haltthreshold >= 0.f) {
		// Check if it is time to run the convergence test again
		const unsigned int pixelCount = imgWidth * imgHeight;
		const double now = WallClockTime();

		// Do not run the test if we don't have at least 16 new samples per pixel
		if ((samplesCount  - lastConvergenceTestSamplesCount > pixelCount * 16) &&
				((now - lastConvergenceTestTime) * 1000.0 >= renderConfig->GetScreenRefreshInterval())) {
			film->UpdateScreenBuffer(); // Required in order to have a valid convergence test
			convergence = 1.f - film->RunConvergenceTest() / (float)pixelCount;
			lastConvergenceTestTime = now;
			lastConvergenceTestSamplesCount = samplesCount;
		}
	}
}

unsigned int PathOCLRenderEngine::GetPass() const {
	return samplesCount / (film->GetWidth() * film->GetHeight());
}

float PathOCLRenderEngine::GetConvergence() const {
	return convergence;
}

bool PathOCLRenderEngine::IsMaterialCompiled(const MaterialType type) const {
	return (compiledScene == NULL) ? false : compiledScene->IsMaterialCompiled(type);
}

#endif
