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
#include <string>
#include <sstream>
#include <stdexcept>

#include "smalllux.h"
#include "path.h"
#include "renderconfig.h"
#include "samplebuffer.h"
#include "displayfunc.h"

PathIntegrator::PathIntegrator(Scene *s, Sampler *samp, SampleBuffer *sb) :
	sampler(samp), scene(s), sampleBuffer(sb) {
	statsRenderingStart = WallClockTime();
	statsTotalSampleCount = 0;
}

PathIntegrator::~PathIntegrator() {
	for (size_t i = 0; i < paths.size(); ++i)
		delete paths[i];
}

void PathIntegrator::ReInit() {
	for (size_t i = 0; i < paths.size(); ++i)
		paths[i]->Init(scene, sampler);
	firstPath = 0;

	statsRenderingStart = WallClockTime();
	statsTotalSampleCount = 0;
}

void PathIntegrator::ClearPaths() {
	for (size_t i = 0; i < paths.size(); ++i)
		delete paths[i];
	paths.clear();
}

void PathIntegrator::FillRayBuffer(RayBuffer *rayBuffer) {
	if (paths.size() == 0) {
		// Need at least 2 paths
		paths.push_back(new Path(scene));
		paths.push_back(new Path(scene));
		paths[0]->Init(scene, sampler);
		paths[1]->Init(scene, sampler);
		firstPath = 0;
	}

	// Worst case: shadow rays count + 1 path ray
	const unsigned int maxRaysPerPath = Path::GetMaxShadowRaysCount(scene) + 1;
	bool allPathDone = true;
	lastPath = firstPath;
	for (;;) {
		paths[lastPath]->FillRayBuffer(rayBuffer);

		if (rayBuffer->LeftSpace() < maxRaysPerPath) {
			allPathDone = false;
			break;
		}

		lastPath = (lastPath + 1) % paths.size();
		if (lastPath == firstPath)
			break;
	}

	if (allPathDone) {
		// Need to add more paths
		size_t newPaths = 0;

		// To limit the number of new paths generated at first run
		const size_t maxNewPaths = rayBuffer->GetSize() >> 3;

		while (rayBuffer->LeftSpace() >= maxRaysPerPath) {
			newPaths++;
			if (newPaths > maxNewPaths)
				break;

			// Add a new path
			Path *p = new Path(scene);
			paths.push_back(p);
			p->Init(scene, sampler);
			p->FillRayBuffer(rayBuffer);
		}

		lastPath = (firstPath - 1) % paths.size();
	}
}

void PathIntegrator::AdvancePaths(const RayBuffer *rayBuffer) {
	for (int i = firstPath; i != lastPath; i = (i + 1) % paths.size()) {
		paths[i]->AdvancePath(scene, sampler, rayBuffer, sampleBuffer);

		// Check if the sample buffer is full
		if (sampleBuffer->IsFull()) {
			statsTotalSampleCount += sampleBuffer->GetSampleCount();

			// Splat all samples on the film
			scene->camera->film->SplatSampleBuffer(sampleBuffer);
			sampleBuffer->Reset();
		}
	}

	firstPath = (lastPath + 1) % paths.size();
}
