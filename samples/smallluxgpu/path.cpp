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

#include "luxrays/core/pixel/samplebuffer.h"

#include "smalllux.h"
#include "path.h"
#include "renderconfig.h"
#include "displayfunc.h"

//------------------------------------------------------------------------------
// Path
//------------------------------------------------------------------------------

bool Path::FillRayBuffer(SLGScene *scene, RayBuffer *rayBuffer) {
	const unsigned int leftSpace = rayBuffer->LeftSpace();
	// Check if the there is enough free space in the RayBuffer
	if (((state == EYE_VERTEX) && (1 > leftSpace)) ||
			((state == EYE_VERTEX_VOLUME_STEP) && (volumeComp->GetRayCount() + 1 > leftSpace)) ||
			((state == ONLY_SHADOW_RAYS) && (tracedShadowRayCount > leftSpace)) ||
			((state == TRANSPARENT_SHADOW_RAYS_STEP) && (tracedShadowRayCount > leftSpace)) ||
			((state == TRANSPARENT_ONLY_SHADOW_RAYS_STEP) && (tracedShadowRayCount > leftSpace)) ||
			((state == NEXT_VERTEX) && (tracedShadowRayCount + 1 > leftSpace)))
		return false;

	if (state == EYE_VERTEX_VOLUME_STEP) {
		volumeComp->AddRays(rayBuffer);
		return true;
	}

	if ((state == EYE_VERTEX) || (state == NEXT_VERTEX))
		currentPathRayIndex = rayBuffer->AddRay(pathRay);

	if ((state == NEXT_VERTEX) || (state == ONLY_SHADOW_RAYS) ||
			(state == TRANSPARENT_SHADOW_RAYS_STEP) || (state == TRANSPARENT_ONLY_SHADOW_RAYS_STEP)) {
		for (unsigned int i = 0; i < tracedShadowRayCount; ++i)
			currentShadowRayIndex[i] = rayBuffer->AddRay(shadowRay[i]);
	}

	return true;
}

void Path::AdvancePath(SLGScene *scene, Film *film, Sampler *sampler, const RayBuffer *rayBuffer,
		SampleBuffer *sampleBuffer) {
	//--------------------------------------------------------------------------
	// Select the path ray hit
	//--------------------------------------------------------------------------

	const RayHit *rayHit = NULL;
	switch (state) {
		case EYE_VERTEX:
			if (scene->volumeIntegrator) {
				rayHit = rayBuffer->GetRayHit(currentPathRayIndex);

				// Use Russian Roulette to check if I have to do participating media computation or not
				if (sample.GetLazyValue() <= scene->volumeIntegrator->GetRRProbability()) {
					Ray volumeRay(pathRay.o, pathRay.d, 0.f, (rayHit->index == 0xffffffffu) ? std::numeric_limits<float>::infinity() : rayHit->t);
					scene->volumeIntegrator->GenerateLiRays(scene, &sample, volumeRay, volumeComp);
					radiance += volumeComp->GetEmittedLight();

					if (volumeComp->GetRayCount() > 0) {
						// Do the EYE_VERTEX_VOLUME_STEP
						state = EYE_VERTEX_VOLUME_STEP;
						eyeHit = *(rayBuffer->GetRayHit(currentPathRayIndex));
						return;
					}
				}
			} else
				rayHit = rayBuffer->GetRayHit(currentPathRayIndex);
			break;
		case NEXT_VERTEX:
			rayHit = rayBuffer->GetRayHit(currentPathRayIndex);
			break;
		case EYE_VERTEX_VOLUME_STEP:
			// Add scattered light
			radiance += throughput * volumeComp->CollectResults(rayBuffer) / scene->volumeIntegrator->GetRRProbability();

			rayHit = &eyeHit;
			break;
		case TRANSPARENT_SHADOW_RAYS_STEP:
			rayHit = &eyeHit;
			break;
		case TRANSPARENT_ONLY_SHADOW_RAYS_STEP:
		case ONLY_SHADOW_RAYS:
			// Nothing
			break;
		default:
			assert(false);
			break;
	}

	//--------------------------------------------------------------------------
	// Finish direct light sampling
	//--------------------------------------------------------------------------

	if (((state == NEXT_VERTEX) ||
			(state == ONLY_SHADOW_RAYS) ||
			(state == TRANSPARENT_SHADOW_RAYS_STEP) ||
			(state == TRANSPARENT_ONLY_SHADOW_RAYS_STEP)) &&
			(tracedShadowRayCount > 0)) {
		unsigned int leftShadowRaysToTrace = 0;
		for (unsigned int i = 0; i < tracedShadowRayCount; ++i) {
			const RayHit *shadowRayHit = rayBuffer->GetRayHit(currentShadowRayIndex[i]);
			if (shadowRayHit->Miss()) {
				// Nothing was hit, light is visible
				radiance += lightColor[i] / lightPdf[i];
			} else {
				// Something was hit check if it is transparent
				const unsigned int currentShadowTriangleIndex = shadowRayHit->index;
				Material *triMat = scene->triangleMaterials[currentShadowTriangleIndex];

				if (triMat->IsShadowTransparent()) {
					// It is shadow transparent, I need to continue to trace the ray
					shadowRay[leftShadowRaysToTrace] = Ray(
							shadowRay[i](shadowRayHit->t),
							shadowRay[i].d,
							shadowRay[i].mint,
							shadowRay[i].maxt - shadowRayHit->t);

					lightColor[leftShadowRaysToTrace] = lightColor[i] * triMat->GetSahdowTransparency();
					lightPdf[leftShadowRaysToTrace] = lightPdf[i];
					leftShadowRaysToTrace++;
				} else {
					// Check if there is a texture with alpha
					TexMapInstance *tm = scene->triangleTexMaps[currentShadowTriangleIndex];

					if (tm) {
						const TextureMap *map = tm->GetTexMap();

						if (map->HasAlpha()) {
							const ExtTriangleMesh *mesh = scene->objects[scene->dataSet->GetMeshID(currentShadowTriangleIndex)];
							const Triangle &tri = mesh->GetTriangles()[scene->dataSet->GetMeshTriangleID(currentShadowTriangleIndex)];
							const UV triUV = InterpolateTriUV(tri, mesh->GetUVs(), shadowRayHit->b1, shadowRayHit->b2);

							const float alpha = map->GetAlpha(triUV);

							if (alpha < 1.f) {
								// It is shadow transparent, I need to continue to trace the ray
								shadowRay[leftShadowRaysToTrace] = Ray(
										shadowRay[i](shadowRayHit->t),
										shadowRay[i].d,
										shadowRay[i].mint,
										shadowRay[i].maxt - shadowRayHit->t);

								lightColor[leftShadowRaysToTrace] = lightColor[i] * (1.f - alpha);
								lightPdf[leftShadowRaysToTrace] = lightPdf[i];
								leftShadowRaysToTrace++;
							}
						}
					}
				}
			}
		}

		if (leftShadowRaysToTrace > 0) {
			tracedShadowRayCount = leftShadowRaysToTrace;
			if ((state == ONLY_SHADOW_RAYS) || (state == TRANSPARENT_ONLY_SHADOW_RAYS_STEP))
				state = TRANSPARENT_ONLY_SHADOW_RAYS_STEP;
			else {
				eyeHit = *rayHit;
				state = TRANSPARENT_SHADOW_RAYS_STEP;
			}

			return;
		}
	}

	//--------------------------------------------------------------------------
	// Calculate next step
	//--------------------------------------------------------------------------

	depth++;

	const bool missed = rayHit ? rayHit->Miss() : false;
	if (missed ||
			(state == ONLY_SHADOW_RAYS) ||
			(state == TRANSPARENT_ONLY_SHADOW_RAYS_STEP) ||
			(depth >= scene->maxPathDepth)) {
		if (missed && scene->infiniteLight && (scene->useInfiniteLightBruteForce || specularBounce)) {
			// Add the light emitted by the infinite light
			radiance += scene->infiniteLight->Le(pathRay.d) * throughput;
		}

		// Hit nothing/only shadow rays/maxdepth, terminate the path
		sampleBuffer->SplatSample(sample.screenX, sample.screenY, radiance);
		// Restart the path
		Init(scene, film, sampler);
		return;
	}

	// Something was hit
	const unsigned int currentTriangleIndex = rayHit->index;

	// Get the triangle
	const ExtTriangleMesh *mesh = scene->objects[scene->dataSet->GetMeshID(currentTriangleIndex)];
	const Triangle &tri = mesh->GetTriangles()[scene->dataSet->GetMeshTriangleID(currentTriangleIndex)];

	// Get the material
	const Material *triMat = scene->triangleMaterials[currentTriangleIndex];

	// Check if it is a light source
	if (triMat->IsLightSource()) {
		if (specularBounce) {
			// Only TriangleLight can be directly hit
			const TriangleLight *tLight = (TriangleLight *) triMat;
			Spectrum Le = tLight->Le(scene, -pathRay.d);

			radiance += Le * throughput;
		}

		// Terminate the path
		sampleBuffer->SplatSample(sample.screenX, sample.screenY, radiance);
		// Restart the path
		Init(scene, film, sampler);
		return;
	}

	//--------------------------------------------------------------------------
	// Build the shadow rays (if required)
	//--------------------------------------------------------------------------

	// Interpolate face normal
	Normal N = InterpolateTriNormal(tri, mesh->GetNormal(), rayHit->b1, rayHit->b2);

	const SurfaceMaterial *triSurfMat = (SurfaceMaterial *) triMat;
	const Point hitPoint = pathRay(rayHit->t);
	const Vector wo = -pathRay.d;

	Spectrum surfaceColor;
	const Spectrum *colors = mesh->GetColors();
	if (colors)
		surfaceColor = InterpolateTriColor(tri, colors, rayHit->b1, rayHit->b2);
	else
		surfaceColor = Spectrum(1.f, 1.f, 1.f);

	// Check if I have to apply texture mapping or normal mapping
	TexMapInstance *tm = scene->triangleTexMaps[currentTriangleIndex];
	BumpMapInstance *bm = scene->triangleBumpMaps[currentTriangleIndex];
	NormalMapInstance *nm = scene->triangleNormalMaps[currentTriangleIndex];
	if (tm || bm || nm) {
		// Interpolate UV coordinates if required
		const UV triUV = InterpolateTriUV(tri, mesh->GetUVs(), rayHit->b1, rayHit->b2);

		// Check if there is an assigned texture map
		if (tm) {
			const TextureMap *map = tm->GetTexMap();

			// Apply texture mapping
			surfaceColor *= map->GetColor(triUV);

			// Check if the texture map has an alpha channel
			if (map->HasAlpha()) {
				const float alpha = map->GetAlpha(triUV);

				if ((alpha == 0.0f) || ((alpha < 1.f) && (sample.GetLazyValue() > alpha))) {
					pathRay = Ray(pathRay(rayHit->t), pathRay.d);
					state = NEXT_VERTEX;
					tracedShadowRayCount = 0;
					return;
				}
			}
		}

		// Check if there is an assigned bump/normal map
		if (bm || nm) {
			if (nm) {
				// Apply normal mapping
				const Spectrum color = nm->GetTexMap()->GetColor(triUV);

				const float x = 2.0 * (color.r - 0.5);
				const float y = 2.0 * (color.g - 0.5);
				const float z = 2.0 * (color.b - 0.5);

				Vector v1, v2;
				CoordinateSystem(Vector(N), &v1, &v2);
				N = Normalize(Normal(
						v1.x * x + v2.x * y + N.x * z,
						v1.y * x + v2.y * y + N.y * z,
						v1.z * x + v2.z * y + N.z * z));
			}

			if (bm) {
				// Apply bump mapping
				const TextureMap *map = bm->GetTexMap();
				const UV &dudv = map->GetDuDv();

				const float b0 = map->GetColor(triUV).Filter();

				const UV uvdu(triUV.u + dudv.u, triUV.v);
				const float bu = map->GetColor(uvdu).Filter();

				const UV uvdv(triUV.u, triUV.v + dudv.v);
				const float bv = map->GetColor(uvdv).Filter();

				const float scale = bm->GetScale();
				const Vector bump(scale * (bu - b0), scale * (bv - b0), 1.f);

				Vector v1, v2;
				CoordinateSystem(Vector(N), &v1, &v2);
				N = Normalize(Normal(
						v1.x * bump.x + v2.x * bump.y + N.x * bump.z,
						v1.y * bump.x + v2.y * bump.y + N.y * bump.z,
						v1.z * bump.x + v2.z * bump.y + N.z * bump.z));
			}
		}
	}

	// Flip the normal if required
	Normal shadeN;
	float RdotShadeN = Dot(pathRay.d, N);

	// Flip shade  normal if required
	if (RdotShadeN > 0.f) {
		// Flip shade  normal
		shadeN = -N;
	} else {
		shadeN = N;
		RdotShadeN = -RdotShadeN;
	}

	tracedShadowRayCount = 0;
	if (triSurfMat->IsDiffuse() && (scene->lights.size() > 0)) {
		// Direct light sampling: trace shadow rays

		switch (scene->lightStrategy) {
			case ALL_UNIFORM:
			{
				// ALL UNIFORM direct sampling light strategy
				const Spectrum lightTroughtput = throughput * surfaceColor;

				for (unsigned int j = 0; j < scene->lights.size(); ++j) {
					// Select the light to sample
					const LightSource *light = scene->lights[j];

					for (unsigned int i = 0; i < scene->shadowRayCount; ++i) {
						// Select a point on the light surface
						lightColor[tracedShadowRayCount] = light->Sample_L(
								scene, hitPoint, &shadeN,
								sample.GetLazyValue(), sample.GetLazyValue(), sample.GetLazyValue(),
								&lightPdf[tracedShadowRayCount], &shadowRay[tracedShadowRayCount]);

						// Scale light pdf for ALL_UNIFORM strategy
						lightPdf[tracedShadowRayCount] *= scene->shadowRayCount;

						if (lightPdf[tracedShadowRayCount] <= 0.0f)
							continue;

						const Vector lwi = shadowRay[tracedShadowRayCount].d;
						lightColor[tracedShadowRayCount] *= lightTroughtput * Dot(shadeN, lwi) *
								triSurfMat->f(wo, lwi, shadeN);

						if (!lightColor[tracedShadowRayCount].Black())
							tracedShadowRayCount++;
					}
				}

				break;
			}
			case ONE_UNIFORM:
			{
				// ONE UNIFORM direct sampling light strategy
				const Spectrum lightTroughtput = throughput * surfaceColor;

				for (unsigned int i = 0; i < scene->shadowRayCount; ++i) {
					// Select the light to sample
					const unsigned int currentLightIndex = scene->SampleLights(sample.GetLazyValue());
					const LightSource *light = scene->lights[currentLightIndex];

					// Select a point on the light surface
					lightColor[tracedShadowRayCount] = light->Sample_L(
							scene, hitPoint, &shadeN,
							sample.GetLazyValue(), sample.GetLazyValue(), sample.GetLazyValue(),
							&lightPdf[tracedShadowRayCount], &shadowRay[tracedShadowRayCount]);

					if (lightPdf[tracedShadowRayCount] <= 0.0f)
						continue;

					const Vector lwi = shadowRay[tracedShadowRayCount].d;
					lightColor[tracedShadowRayCount] *= lightTroughtput * Dot(shadeN, lwi) *
							triSurfMat->f(wo, lwi, shadeN);

					if (!lightColor[tracedShadowRayCount].Black())
						tracedShadowRayCount++;
				}

				// Update the light pdf according the number of shadow ray traced
				const float lightStrategyPdf = static_cast<float> (tracedShadowRayCount) / static_cast<float> (scene->lights.size());
				for (unsigned int i = 0; i < tracedShadowRayCount; ++i) {
					// Scale light pdf for ONE_UNIFORM strategy
					lightPdf[i] *= lightStrategyPdf;
				}

				break;
			}
			default:
				assert(false);
		}
	}

	//--------------------------------------------------------------------------
	// Build the next vertex path ray
	//--------------------------------------------------------------------------

	float fPdf;
	Vector wi;
	const Spectrum f = triSurfMat->Sample_f(wo, &wi, N, shadeN,
			sample.GetLazyValue(), sample.GetLazyValue(), sample.GetLazyValue(),
			scene->onlySampleSpecular, &fPdf, specularBounce) * surfaceColor;
	if ((fPdf <= 0.f) || f.Black()) {
		if (tracedShadowRayCount > 0)
			state = ONLY_SHADOW_RAYS;
		else {
			// Terminate the path
			sampleBuffer->SplatSample(sample.screenX, sample.screenY, radiance);
			// Restart the path
			Init(scene, film, sampler);
		}

		return;
	}

	throughput *= f / fPdf;

	if (depth > scene->rrDepth) {
		// Russian Roulette
		switch (scene->rrStrategy) {
			case PROBABILITY:
			{
				if (scene->rrProb >= sample.GetLazyValue())
					throughput /= scene->rrProb;
				else {
					// Check if terminate the path or I have still to trace shadow rays
					if (tracedShadowRayCount > 0)
						state = ONLY_SHADOW_RAYS;
					else {
						// Terminate the path
						sampleBuffer->SplatSample(sample.screenX, sample.screenY, radiance);
						// Restart the path
						Init(scene, film, sampler);
					}

					return;
				}
				break;
			}
			case IMPORTANCE:
			{
				const float prob = Max(throughput.Filter(), scene->rrImportanceCap);
				if (prob >= sample.GetLazyValue())
					throughput /= prob;
				else {
					// Check if terminate the path or I have still to trace shadow rays
					if (tracedShadowRayCount > 0)
						state = ONLY_SHADOW_RAYS;
					else {
						// Terminate the path
						sampleBuffer->SplatSample(sample.screenX, sample.screenY, radiance);
						// Restart the path
						Init(scene, film, sampler);
					}

					return;
				}
				break;
			}
			default:
				assert(false);
		}
	}

	pathRay.o = hitPoint;
	pathRay.d = wi;
	state = NEXT_VERTEX;
}

//------------------------------------------------------------------------------
// Path Integrator
//------------------------------------------------------------------------------

PathIntegrator::PathIntegrator(SLGScene *s, Film *f, Sampler *samp) :
	scene(s), film(f), sampler(samp) {
	sampleBuffer = film->GetFreeSampleBuffer();
	statsRenderingStart = WallClockTime();
	statsTotalSampleCount = 0;
}

PathIntegrator::~PathIntegrator() {
	film->FreeSampleBuffer(sampleBuffer);

	for (size_t i = 0; i < paths.size(); ++i)
		delete paths[i];
}

void PathIntegrator::ReInit() {
	for (size_t i = 0; i < paths.size(); ++i)
		paths[i]->Init(scene, film, sampler);
	firstPath = 0;
	sampleBuffer->Reset();

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
		paths[0]->Init(scene, film, sampler);
		paths[1]->Init(scene, film, sampler);
		firstPath = 0;
	}

	bool allPathDone = true;
	lastPath = firstPath;
	for (;;) {
		if (!paths[lastPath]->FillRayBuffer(scene, rayBuffer)) {
			// Not enough free space in the RayBuffer
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

		for (;;) {
			newPaths++;
			if (newPaths > maxNewPaths) {
				firstPath = 0;
				lastPath = paths.size() - 1;
				break;
			}

			// Add a new path
			Path *p = new Path(scene);
			paths.push_back(p);
			p->Init(scene, film, sampler);
			if (!p->FillRayBuffer(scene, rayBuffer)) {
				firstPath = 0;
				// -2 because the addition of the last path failed
				lastPath = paths.size() - 2;
				break;
			}
		}
	}
}

void PathIntegrator::AdvancePaths(const RayBuffer *rayBuffer) {
	for (int i = firstPath; i != lastPath; i = (i + 1) % paths.size()) {
		paths[i]->AdvancePath(scene, film, sampler, rayBuffer, sampleBuffer);

		// Check if the sample buffer is full
		if (sampleBuffer->IsFull()) {
			statsTotalSampleCount += sampleBuffer->GetSampleCount();

			// Splat all samples on the film
			film->SplatSampleBuffer(sampler->IsPreviewOver(), sampleBuffer);
			sampleBuffer = film->GetFreeSampleBuffer();
		}
	}

	firstPath = (lastPath + 1) % paths.size();
}

//------------------------------------------------------------------------------
// PathRenderThread
//------------------------------------------------------------------------------

PathRenderThread::PathRenderThread(unsigned int index, SLGScene *scn, Film *f) {
	threadIndex = index;
	scene = scn;
	film = f;
	started = false;
}

PathRenderThread::~PathRenderThread() {
}

//------------------------------------------------------------------------------
// PathNativeRenderThread
//------------------------------------------------------------------------------

PathNativeRenderThread::PathNativeRenderThread(unsigned int index,  const unsigned long seedBase,
		const float samplStart, const unsigned int samplePerPixel,
		NativeThreadIntersectionDevice *device,
		SLGScene *scn, Film *f, const bool lowLatency) : PathRenderThread(index, scn, f) {
	intersectionDevice = device;
	samplingStart = samplStart;

	// Allocate buffers

	// Ray buffer (small buffers work well with CPU)
	const size_t rayBufferSize = 1024;
	const unsigned int startLine = Clamp<unsigned int>(
		film->GetHeight() * samplingStart,
			0, film->GetHeight() - 1);

	sampler = new RandomSampler(lowLatency, seedBase + threadIndex + 1,
			film->GetWidth(), film->GetHeight(),
			samplePerPixel, startLine);

	pathIntegrator = new PathIntegrator(scene, film, sampler);
	rayBuffer = new RayBuffer(rayBufferSize);

	renderThread = NULL;
}

PathNativeRenderThread::~PathNativeRenderThread() {
	if (started)
		Stop();

	delete rayBuffer;
	delete pathIntegrator;
	delete sampler;
}

void PathNativeRenderThread::Start() {
	PathRenderThread::Start();

	const unsigned int startLine = Clamp<unsigned int>(
		film->GetHeight() * samplingStart,
			0, film->GetHeight() - 1);
	sampler->Init(film->GetWidth(), film->GetHeight(), startLine);
	rayBuffer->Reset();
	pathIntegrator->ReInit();

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(PathNativeRenderThread::RenderThreadImpl, this));
}

void PathNativeRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void PathNativeRenderThread::Stop() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}

	PathRenderThread::Stop();
}

void PathNativeRenderThread::ClearPaths() {
	pathIntegrator->ClearPaths();
}

void PathNativeRenderThread::RenderThreadImpl(PathNativeRenderThread *renderThread) {
	cerr << "[PathNativeRenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	try {
		RayBuffer *rayBuffer = renderThread->rayBuffer;
		PathIntegrator *pathIntegrator = renderThread->pathIntegrator;
		NativeThreadIntersectionDevice *intersectionDevice = renderThread->intersectionDevice;

		while (!boost::this_thread::interruption_requested()) {
			rayBuffer->Reset();
			pathIntegrator->FillRayBuffer(rayBuffer);
			intersectionDevice->Intersect(rayBuffer);
			pathIntegrator->AdvancePaths(rayBuffer);
		}

		cerr << "[PathNativeRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[PathNativeRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	}
#if !defined(LUXRAYS_DISABLE_OPENCL)
	catch (cl::Error err) {
		cerr << "[PathNativeRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}
#endif
}

//------------------------------------------------------------------------------
// PathDeviceRenderThread
//------------------------------------------------------------------------------

PathDeviceRenderThread::PathDeviceRenderThread(const unsigned int index, const unsigned long seedBase,
		const float samplStart,  const unsigned int samplePerPixel, IntersectionDevice *device,
		SLGScene *scn, Film *f, const bool lowLatency) : PathRenderThread(index, scn, f) {
	intersectionDevice = device;
	samplingStart = samplStart;
	reportedPermissionError = false;

	// Allocate buffers

	// Ray buffer
	// TODO: cross check RAY_BUFFER_SIZE with the Intersection device
	const size_t rayBufferSize = lowLatency ? (RAY_BUFFER_SIZE / 8) : RAY_BUFFER_SIZE;
	const unsigned int startLine = Clamp<unsigned int>(
		film->GetHeight() * samplingStart,
			0, film->GetHeight() - 1);

	sampler = new RandomSampler(lowLatency, seedBase + threadIndex + 1,
			film->GetWidth(), film->GetHeight(),
			samplePerPixel, startLine);

	for(size_t i = 0; i < PATH_DEVICE_RENDER_BUFFER_COUNT; i++) {
		pathIntegrators[i] = new PathIntegrator(scene, film, sampler);
		rayBuffers[i] = new RayBuffer(rayBufferSize);
		rayBuffers[i]->PushUserData(i);
	}

	renderThread = NULL;
}

PathDeviceRenderThread::~PathDeviceRenderThread() {
	if (started)
		Stop();

	for(size_t i = 0; i < PATH_DEVICE_RENDER_BUFFER_COUNT; i++) {
		delete rayBuffers[i];
		delete pathIntegrators[i];
	}
	delete sampler;
}

void PathDeviceRenderThread::Start() {
	PathRenderThread::Start();

	const unsigned int startLine = Clamp<unsigned int>(
			film->GetHeight() * samplingStart,
			0, film->GetHeight() - 1);
	sampler->Init(film->GetWidth(), film->GetHeight(), startLine);
	for (size_t i = 0; i < PATH_DEVICE_RENDER_BUFFER_COUNT; i++) {
		rayBuffers[i]->Reset();
		rayBuffers[i]->ResetUserData();
		rayBuffers[i]->PushUserData(i);
		pathIntegrators[i]->ReInit();
	}

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(PathDeviceRenderThread::RenderThreadImpl, this));

	// Set renderThread priority
	bool res = SetThreadRRPriority(renderThread);
	if (res && !reportedPermissionError) {
		cerr << "[PathNativeRenderThread::" << threadIndex << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)" << endl;
		reportedPermissionError = true;
	}
}

void PathDeviceRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void PathDeviceRenderThread::Stop() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}

	PathRenderThread::Stop();
}

void PathDeviceRenderThread::ClearPaths() {
	assert (!started);

	for(size_t i = 0; i < PATH_DEVICE_RENDER_BUFFER_COUNT; i++)
		pathIntegrators[i]->ClearPaths();
}

void PathDeviceRenderThread::RenderThreadImpl(PathDeviceRenderThread *renderThread) {
	cerr << "[PathDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	std::deque<RayBuffer *> todoBuffers;
	for(size_t i = 0; i < PATH_DEVICE_RENDER_BUFFER_COUNT; i++)
		todoBuffers.push_back(renderThread->rayBuffers[i]);

	try {
		while (!boost::this_thread::interruption_requested()) {
			// Produce buffers to trace
			while (todoBuffers.size() > 0) {
				RayBuffer *rayBuffer = todoBuffers.front();
				todoBuffers.pop_front();

				rayBuffer->Reset();
				renderThread->pathIntegrators[rayBuffer->GetUserData()]->FillRayBuffer(rayBuffer);
				renderThread->intersectionDevice->PushRayBuffer(rayBuffer);
			}

			RayBuffer *rayBuffer = renderThread->intersectionDevice->PopRayBuffer();
			renderThread->pathIntegrators[rayBuffer->GetUserData()]->AdvancePaths(rayBuffer);
			todoBuffers.push_back(rayBuffer);
		}

		cerr << "[PathDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[PathDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	}
#if !defined(LUXRAYS_DISABLE_OPENCL)
	catch (cl::Error err) {
		cerr << "[PathDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}
#endif
}

//------------------------------------------------------------------------------
// PathRenderEngine
//------------------------------------------------------------------------------

PathRenderEngine::PathRenderEngine(SLGScene *scn, Film *flm,
		vector<IntersectionDevice *> intersectionDev,
		const bool lowLatency, const unsigned int samplePerPixel) : RenderEngine(scn, flm) {
	intersectionDevices = intersectionDev;

	const unsigned long seedBase = (unsigned long)(WallClockTime() / 1000.0);

	// Create and start render threads
	size_t renderThreadCount = intersectionDevices.size();
	cerr << "Starting "<< renderThreadCount << " Path render threads" << endl;
	for (size_t i = 0; i < renderThreadCount; ++i) {
		if (intersectionDevices[i]->GetType() == DEVICE_TYPE_NATIVE_THREAD) {
			PathNativeRenderThread *t = new PathNativeRenderThread(i, seedBase, i / (float)renderThreadCount,
					samplePerPixel, (NativeThreadIntersectionDevice *)intersectionDevices[i], scene, film, lowLatency);
			renderThreads.push_back(t);
		} else {
			PathDeviceRenderThread *t = new PathDeviceRenderThread(i, seedBase, i / (float)renderThreadCount,
					samplePerPixel, intersectionDevices[i], scene, film, lowLatency);
			renderThreads.push_back(t);
		}
	}
}

PathRenderEngine::~PathRenderEngine() {
	if (started) {
		Interrupt();
		Stop();
	}

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];
}

void PathRenderEngine::Start() {
	RenderEngine::Start();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
}

void PathRenderEngine::Interrupt() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
}

void PathRenderEngine::Stop() {
	RenderEngine::Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();
}

void PathRenderEngine::Reset() {
	assert (!started);

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->ClearPaths();
}

unsigned int PathRenderEngine::GetPass() const {
	unsigned int pass = 0;

	for (size_t i = 0; i < renderThreads.size(); ++i)
		pass += renderThreads[i]->GetPass();

	return pass;
}

unsigned int PathRenderEngine::GetThreadCount() const {
	return renderThreads.size();
}
