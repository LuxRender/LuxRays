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

#include "smalllux.h"
#include "renderconfig.h"
#include "bidirhybrid/bidirhybrid.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/opencl/utils.h"

#if defined(__APPLE__)
//OSX version detection
#include <sys/utsname.h>
#endif

//------------------------------------------------------------------------------
// BiDirState
//------------------------------------------------------------------------------

static inline float MIS(const float a) {
	//return a; // Balance heuristic
	return a * a; // Power heuristic
}

const unsigned int sampleEyeBootSize = 6;
const unsigned int sampleEyeStepSize = 11;
const unsigned int sampleLightBootSize = 5;
const unsigned int sampleLightStepSize = 6;

BiDirState::BiDirState(BiDirHybridRenderEngine *renderEngine,
		Film *film, RandomGenerator *rndGen) : HybridRenderState(renderEngine, film, rndGen),
		eyeSampleResults(renderEngine->eyePathCount) {

	// Setup the sampler
	const unsigned int sampleSize = 
		renderEngine->eyePathCount * (sampleEyeBootSize + renderEngine->maxEyePathDepth * sampleEyeStepSize) + // For each eye path and eye vertex
		renderEngine->lightPathCount * (sampleLightBootSize + renderEngine->maxLightPathDepth * sampleLightStepSize); // For each light path and light vertex
	sampler->RequestSamples(sampleSize);
}

void BiDirState::ConnectVertices(HybridRenderThread *renderThread,
		const u_int eyePathIndex,
		const PathVertex &eyeVertex, const PathVertex &lightVertex,
		const float u0) {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;

	Vector eyeDir(lightVertex.bsdf.hitPoint - eyeVertex.bsdf.hitPoint);
	const float eyeDistance2 = eyeDir.LengthSquared();
	const float eyeDistance = sqrtf(eyeDistance2);
	eyeDir /= eyeDistance;

	// Check eye vertex BSDF
	float eyeBsdfPdfW, eyeBsdfRevPdfW;
	BSDFEvent eyeEvent;
	Spectrum eyeBsdfEval = eyeVertex.bsdf.Evaluate(eyeDir, &eyeEvent, &eyeBsdfPdfW, &eyeBsdfRevPdfW);

	if (!eyeBsdfEval.Black()) {
		// Check light vertex BSDF
		float lightBsdfPdfW, lightBsdfRevPdfW;
		BSDFEvent lightEvent;
		Spectrum lightBsdfEval = eyeVertex.bsdf.Evaluate(eyeDir, &lightEvent, &lightBsdfPdfW, &lightBsdfRevPdfW);

		if (!lightBsdfEval.Black()) {
			// Check the 2 surfaces can see each other
			const float cosThetaAtCamera = Dot(eyeVertex.bsdf.shadeN, eyeDir);
			const float cosThetaAtLight = Dot(lightVertex.bsdf.shadeN, -eyeDir);
			const float geometryTerm = cosThetaAtLight * cosThetaAtLight / eyeDistance2;
			if (geometryTerm <= 0.f)
				return;

			// Trace  ray between the two vertices
			Ray eyeRay(eyeVertex.bsdf.hitPoint, eyeDir);
			eyeRay.maxt = eyeDistance - MachineEpsilon::E(eyeDistance);

			if (eyeVertex.depth >= renderEngine->rrDepth) {
				// Russian Roulette
				const float prob = Max(eyeBsdfEval.Filter(), renderEngine->rrImportanceCap);
				eyeBsdfPdfW *= prob;
				eyeBsdfRevPdfW *= prob;
			}

			if (lightVertex.depth >= renderEngine->rrDepth) {
				// Russian Roulette
				const float prob = Max(lightBsdfEval.Filter(), renderEngine->rrImportanceCap);
				lightBsdfPdfW *= prob;
				lightBsdfRevPdfW *= prob;
			}

			// Convert pdfs to area pdf
			const float eyeBsdfPdfA = PdfWtoA(eyeBsdfPdfW, eyeDistance, cosThetaAtLight);
			const float lightBsdfPdfA  = PdfWtoA(lightBsdfPdfW,  eyeDistance, cosThetaAtCamera);

			// MIS weights
			const float lightWeight = eyeBsdfPdfA * (lightVertex.dVCM + lightVertex.dVC * MIS(lightBsdfRevPdfW));
			const float eyeWeight = lightBsdfPdfA * (eyeVertex.dVCM + eyeVertex.dVC * MIS(eyeBsdfRevPdfW));

			const float misWeight = 1.f / (renderEngine->lightPathCount * (lightWeight + 1.f + eyeWeight));

			const Spectrum radiance = (misWeight * geometryTerm) * eyeVertex.throughput * eyeBsdfEval *
						lightBsdfEval * lightVertex.throughput;

			// Add the ray to trace and the result
			eyeSampleResults[eyePathIndex].sampleValue.push_back(u0);
			thread->PushRay(eyeRay);
			eyeSampleResults[eyePathIndex].sampleRadiance.push_back(radiance);
		}
	}
}

bool BiDirState::ConnectToEye(HybridRenderThread *renderThread,
		const PathVertex &lightVertex, const float u0,
		const Point &lensPoint) {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	Vector eyeDir(lightVertex.bsdf.hitPoint - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	float bsdfPdfW, bsdfRevPdfW;
	BSDFEvent event;
	Spectrum bsdfEval = lightVertex.bsdf.Evaluate(-eyeDir, &event, &bsdfPdfW, &bsdfRevPdfW);

	if (!bsdfEval.Black()) {
		Ray eyeRay(lensPoint, eyeDir);
		eyeRay.maxt = eyeDistance - MachineEpsilon::E(eyeDistance);

		float scrX, scrY;
		if (scene->camera->GetSamplePosition(lensPoint, eyeDir, eyeDistance, &scrX, &scrY)) {
			if (lightVertex.depth >= renderEngine->rrDepth) {
				// Russian Roulette
				const float prob = Max(bsdfEval.Filter(), renderEngine->rrImportanceCap);
				bsdfPdfW *= prob;
				bsdfRevPdfW *= prob;
			}

			const float cosToCamera = Dot(lightVertex.bsdf.shadeN, -eyeDir);
			const float cosAtCamera = Dot(scene->camera->GetDir(), eyeDir);

			const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
				scene->camera->GetPixelArea());
			const float cameraPdfA = PdfWtoA(cameraPdfW, eyeDistance, cosToCamera);
			const float fluxToRadianceFactor = cameraPdfA;

			// MIS weight (cameraPdfA must be expressed normalized device coordinate)
			const unsigned int pixelCount = thread->threadFilm->GetWidth() * thread->threadFilm->GetHeight();
			const float weightLight = MIS(cameraPdfA / pixelCount) *
					(lightVertex.dVCM + lightVertex.dVC * MIS(bsdfRevPdfW));
			const float misWeight = 1.f / (renderEngine->lightPathCount * (weightLight + 1.f));

			const Spectrum radiance = misWeight * lightVertex.throughput * fluxToRadianceFactor * bsdfEval;

			// Add the ray to trace and the result
			lightSampleValue.push_back(u0);
			thread->PushRay(eyeRay);
			AddSampleResult(lightSampleResults, PER_SCREEN_NORMALIZED, scrX, scrY,
					radiance, 1.f);
			return true;
		}
	}

	return false;
}

void BiDirState::DirectLightSampling(HybridRenderThread *renderThread,
		const u_int eyePathIndex,
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertex &eyeVertex) {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;
	
	if (!eyeVertex.bsdf.IsDelta()) {
		// Pick a light source to sample
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(u0, &lightPickPdf);

		Vector lightRayDir;
		float distance, directPdfW, emissionPdfW, cosThetaAtLight;
		Spectrum lightRadiance = light->Illuminate(scene, eyeVertex.bsdf.hitPoint,
				u1, u2, u3, &lightRayDir, &distance, &directPdfW, &emissionPdfW,
				&cosThetaAtLight);

		if (!lightRadiance.Black()) {
			BSDFEvent event;
			float bsdfPdfW, bsdfRevPdfW;
			Spectrum bsdfEval = eyeVertex.bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW, &bsdfRevPdfW);

			if (!bsdfEval.Black()) {
				Ray shadowRay(eyeVertex.bsdf.hitPoint, lightRayDir,
						MachineEpsilon::E(eyeVertex.bsdf.hitPoint),
						distance - MachineEpsilon::E(distance));

				if (eyeVertex.depth >= renderEngine->rrDepth) {
					// Russian Roulette
					const float prob = Max(bsdfEval.Filter(), renderEngine->rrImportanceCap);
					bsdfPdfW *= prob;
					bsdfRevPdfW *= prob;
				}

				const float cosThetaToLight = AbsDot(lightRayDir, eyeVertex.bsdf.shadeN);
				const float directLightSamplingPdfW = directPdfW * lightPickPdf;

				// emissionPdfA / directPdfA = emissionPdfW / directPdfW
				const float weightLight = MIS(bsdfPdfW / directLightSamplingPdfW);
				const float weightCamera = MIS(emissionPdfW * cosThetaToLight / (directPdfW * cosThetaAtLight)) *
					(eyeVertex.dVCM + eyeVertex.dVC * MIS(bsdfRevPdfW));
				const float misWeight = 1.f / (weightLight + 1.f + weightCamera);

				const float factor = cosThetaToLight / directLightSamplingPdfW;
		
				const Spectrum radiance = (misWeight * factor) * eyeVertex.throughput * lightRadiance * bsdfEval;

				// Add the ray to trace and the result
				eyeSampleResults[eyePathIndex].sampleValue.push_back(u4);
				thread->PushRay(shadowRay);
				eyeSampleResults[eyePathIndex].sampleRadiance.push_back(radiance);
			}
		}
	}
}

void BiDirState::DirectHitLight(HybridRenderThread *renderThread,
		const bool finiteLightSource, const PathVertex &eyeVertex,
		Spectrum *radiance) const {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	float directPdfA, emissionPdfW;
	const Spectrum lightRadiance = (finiteLightSource) ?
		eyeVertex.bsdf.GetEmittedRadiance(scene, &directPdfA, &emissionPdfW) :
		scene->GetEnvLightsRadiance(eyeVertex.bsdf.fixedDir, Point(), &directPdfA, &emissionPdfW);
	if (lightRadiance.Black())
		return;

	if (eyeVertex.depth == 1) {
		*radiance += eyeVertex.throughput * lightRadiance;
		return;
	}

	const float lightPickPdf = scene->PickLightPdf();
	directPdfA *= lightPickPdf;
	emissionPdfW *= lightPickPdf;

	// MIS weight
	const float weightCamera = MIS(directPdfA) * eyeVertex.dVCM +
		MIS(emissionPdfW) * eyeVertex.dVC;
	const float misWeight = 1.f / (weightCamera + 1.f);

	*radiance += misWeight * eyeVertex.throughput * lightRadiance;
}

void BiDirState::TraceLightPath(HybridRenderThread *renderThread,
		Sampler *sampler, const u_int lightPathIndex,
		vector<vector<PathVertex> > &lightPaths) {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	const u_int lightPathSampleOffset = renderEngine->eyePathCount * (sampleEyeBootSize + renderEngine->maxEyePathDepth * sampleEyeStepSize) +
		lightPathIndex * (sampleLightBootSize + renderEngine->maxLightPathDepth * sampleLightStepSize);

	// Select one light source
	float lightPickPdf;
	const LightSource *light = scene->SampleAllLights(sampler->GetSample(lightPathSampleOffset), &lightPickPdf);

	// Initialize the light path
	PathVertex lightVertex;
	float lightEmitPdfW, lightDirectPdfW, cosThetaAtLight;
	Ray lightRay;
	lightVertex.throughput = light->Emit(scene,
			sampler->GetSample(lightPathSampleOffset + 1), sampler->GetSample(lightPathSampleOffset + 2),
			sampler->GetSample(lightPathSampleOffset + 3), sampler->GetSample(lightPathSampleOffset + 4),
			&lightRay.o, &lightRay.d, &lightEmitPdfW, &lightDirectPdfW, &cosThetaAtLight);
	if (!lightVertex.throughput.Black()) {
		lightEmitPdfW *= lightPickPdf;
		lightDirectPdfW *= lightPickPdf;

		lightVertex.throughput /= lightEmitPdfW;
		assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

		// I don't store the light vertex 0 because direct lighting will take
		// care of this kind of paths
		lightVertex.dVCM = MIS(lightDirectPdfW / lightEmitPdfW);
		const float usedCosLight = light->IsEnvironmental() ? 1.f : cosThetaAtLight;
		lightVertex.dVC = MIS(usedCosLight / lightEmitPdfW);

		lightVertex.depth = 1;
		while (lightVertex.depth <= renderEngine->maxLightPathDepth) {
			const unsigned int lightVertexSampleOffset = lightPathSampleOffset + sampleLightBootSize + (lightVertex.depth - 1) * sampleLightStepSize;

			RayHit nextEventRayHit;
			Spectrum connectionThroughput;
			if (scene->Intersect(thread->device, true, true, sampler->GetSample(lightVertexSampleOffset),
					&lightRay, &nextEventRayHit, &lightVertex.bsdf, &connectionThroughput)) {
				// Something was hit

				// Check if it is a light source
				if (lightVertex.bsdf.IsLightSource()) {
					// SLG light sources are like black bodies
					break;
				}

				// Update the new light vertex
				lightVertex.throughput *= connectionThroughput;
				// Infinite lights use MIS based on solid angle instead of area
				if((lightVertex.depth > 1) || !light->IsEnvironmental())
					lightVertex.dVCM *= MIS(nextEventRayHit.t * nextEventRayHit.t);
				const float factor = 1.f / MIS(AbsDot(lightVertex.bsdf.shadeN, lightRay.d));
				lightVertex.dVCM *= factor;
				lightVertex.dVC *= factor;

				// Store the vertex only if it isn't specular
				if (!lightVertex.bsdf.IsDelta())
					lightPaths[lightPathIndex].push_back(lightVertex);

				if (lightVertex.depth >= renderEngine->maxLightPathDepth)
					break;

				//----------------------------------------------------------
				// Build the next vertex path ray
				//----------------------------------------------------------

				if (!Bounce(renderThread, sampler, lightVertexSampleOffset + 2,
						&lightVertex, &lightRay))
					break;

				++(lightVertex.depth);
			} else {
				// Ray lost in space...
				break;
			}
		}
	}
}

bool BiDirState::Bounce(HybridRenderThread *renderThread,
		Sampler *sampler, const u_int sampleOffset,
		PathVertex *pathVertex, Ray *nextEventRay) const {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;

	Vector sampledDir;
	BSDFEvent event;
	float bsdfPdfW, cosSampledDir;
	const Spectrum bsdfSample = pathVertex->bsdf.Sample(&sampledDir,
			sampler->GetSample(sampleOffset),
			sampler->GetSample(sampleOffset + 1),
			sampler->GetSample(sampleOffset + 2),
			&bsdfPdfW, &cosSampledDir, &event);
	if (bsdfSample.Black())
		return false;

	float bsdfRevPdfW;
	if (event & SPECULAR)
		bsdfRevPdfW = bsdfPdfW;
	else
		pathVertex->bsdf.Pdf(sampledDir, NULL, &bsdfRevPdfW);

	if (pathVertex->depth >= renderEngine->rrDepth) {
		// Russian Roulette
		const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
		if (sampler->GetSample(sampleOffset + 3) < prob) {
			bsdfPdfW *= prob;
			bsdfRevPdfW *= prob;
		} else
			return false;
	}

	pathVertex->throughput *= bsdfSample * (cosSampledDir / bsdfPdfW);
	assert (!pathVertex->throughput.IsNaN() && !pathVertex->throughput.IsInf());

	// New MIS weights
	if (event & SPECULAR) {
		pathVertex->dVCM = 0.f;
		pathVertex->dVC *= MIS(cosSampledDir / bsdfPdfW) * MIS(bsdfRevPdfW);
	} else {
		pathVertex->dVC = MIS(cosSampledDir / bsdfPdfW) * (pathVertex->dVC *
				MIS(bsdfRevPdfW) + pathVertex->dVCM);
		pathVertex->dVCM = MIS(1.f / bsdfPdfW);
	}

	*nextEventRay = Ray(pathVertex->bsdf.hitPoint, sampledDir);

	return true;
}

void BiDirState::GenerateRays(HybridRenderThread *renderThread) {
	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = thread->threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();
	const unsigned int pixelCount = filmWidth * filmHeight;

	lightSampleValue.clear();
	lightSampleResults.clear();
	for (u_int eyePathIndex = 0; eyePathIndex < renderEngine->eyePathCount; ++eyePathIndex) {
		eyeSampleResults[eyePathIndex].lightPathVertexConnections = 0;
		eyeSampleResults[eyePathIndex].radiance = Spectrum();
		eyeSampleResults[eyePathIndex].sampleValue.clear();
		eyeSampleResults[eyePathIndex].sampleRadiance.clear();
	}

	//--------------------------------------------------------------------------
	// Build the set of light paths
	//--------------------------------------------------------------------------

	vector<vector<PathVertex> > lightPaths(renderEngine->lightPathCount);
	for (u_int lightPathIndex = 0; lightPathIndex < renderEngine->lightPathCount; ++lightPathIndex) {
		//----------------------------------------------------------------------
		// Trace a new light path
		//----------------------------------------------------------------------

		TraceLightPath(renderThread, sampler, lightPathIndex, lightPaths);
	}

	//--------------------------------------------------------------------------
	// Build the set of eye paths and trace rays
	//--------------------------------------------------------------------------

	for (u_int eyePathIndex = 0; eyePathIndex < renderEngine->eyePathCount; ++eyePathIndex) {
		//----------------------------------------------------------------------
		// Trace eye path
		//----------------------------------------------------------------------

		const u_int eyePathSampleOffset = eyePathIndex * (sampleEyeBootSize + renderEngine->maxEyePathDepth * sampleEyeStepSize);

		// Sample a point on the camera lens
		Point lensPoint;
		if (!camera->SampleLens(sampler->GetSample(eyePathSampleOffset + 2), sampler->GetSample(eyePathSampleOffset + 3),
				&lensPoint))
			return;

		//----------------------------------------------------------------------
		// Trace all connections between all light paths and the first eye
		// vertex
		//----------------------------------------------------------------------

		if (eyePathIndex == 0) {
			// As suggested in the CBDPT paper (paragraph 3.2). I do connect the light vertices
			// only with the first eye vertex of the first path. This because, in a pinhole camera,
			// all the first vertices are the same (they are slightly different only if the lens radius
			// is > 0.0).
			for (u_int lightPathIndex = 0; lightPathIndex < lightPaths.size(); ++lightPathIndex) {
				const u_int lightPathSampleOffset = renderEngine->eyePathCount * (sampleEyeBootSize + renderEngine->maxEyePathDepth * sampleEyeStepSize) +
					lightPathIndex * (sampleLightBootSize + renderEngine->maxLightPathDepth * sampleLightStepSize);

				for (u_int lightVertexIndex = 0; lightVertexIndex < lightPaths[lightPathIndex].size(); ++lightVertexIndex) {
					const unsigned int lightVertexSampleOffset = lightPathSampleOffset + sampleLightBootSize + lightVertexIndex * sampleLightStepSize;

					//--------------------------------------------------------------
					// Try to connect the light path vertex with the eye
					//--------------------------------------------------------------
					if (ConnectToEye(renderThread, lightPaths[lightPathIndex][lightVertexIndex],
							sampler->GetSample(lightVertexSampleOffset + 1), lensPoint))
						eyeSampleResults[eyePathIndex].lightPathVertexConnections += 1;
				}
			}
		}

		PathVertex eyeVertex;
		eyeSampleResults[eyePathIndex].alpha = 1.f;

		Ray eyeRay;
		eyeSampleResults[eyePathIndex].screenX = min(sampler->GetSample(eyePathSampleOffset) * filmWidth, (float)(filmWidth - 1));
		eyeSampleResults[eyePathIndex].screenY = min(sampler->GetSample(eyePathSampleOffset + 1) * filmHeight, (float)(filmHeight - 1));
		camera->GenerateRay(eyeSampleResults[eyePathIndex].screenX, eyeSampleResults[eyePathIndex].screenY, &eyeRay,
			sampler->GetSample(eyePathSampleOffset + 4), sampler->GetSample(eyePathSampleOffset + 5));

		eyeVertex.bsdf.fixedDir = -eyeRay.d;
		eyeVertex.throughput = Spectrum(1.f, 1.f, 1.f);
		const float cosAtCamera = Dot(scene->camera->GetDir(), eyeRay.d);
		const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
			scene->camera->GetPixelArea() * pixelCount);
		eyeVertex.dVCM = MIS(1.f / cameraPdfW);
		eyeVertex.dVC = 0.f;

		eyeVertex.depth = 1;
		while (eyeVertex.depth <= renderEngine->maxEyePathDepth) {
			const unsigned int eyeVertexSampleOffset = eyePathSampleOffset + sampleEyeBootSize + (eyeVertex.depth - 1) * sampleEyeStepSize;

			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			if (!scene->Intersect(thread->device, false, true, sampler->GetSample(eyeVertexSampleOffset),
					&eyeRay, &eyeRayHit, &eyeVertex.bsdf, &connectionThroughput)) {
				// Nothing was hit, look for infinitelight

				// This is a trick, you can not have a BSDF of something that has
				// not been hit. DirectHitInfiniteLight must be aware of this.
				eyeVertex.bsdf.fixedDir = -eyeRay.d;
				eyeVertex.throughput *= connectionThroughput;

				DirectHitLight(renderThread, false, eyeVertex, &eyeSampleResults[eyePathIndex].radiance);

				if (eyeVertex.depth == 1)
					eyeSampleResults[eyePathIndex].alpha = 0.f;
				break;
			}
			eyeVertex.throughput *= connectionThroughput;

			// Something was hit

			// Update MIS constants
			eyeVertex.dVCM *= MIS(eyeRayHit.t * eyeRayHit.t);
			const float factor = 1.f / MIS(AbsDot(eyeVertex.bsdf.shadeN, eyeVertex.bsdf.fixedDir));
			eyeVertex.dVCM *= factor;
			eyeVertex.dVC *= factor;

			// Check if it is a light source
			if (eyeVertex.bsdf.IsLightSource()) {
				DirectHitLight(renderThread, true, eyeVertex, &eyeSampleResults[eyePathIndex].radiance);

				// SLG light sources are like black bodies
				break;
			}

			// Note: pass-through check is done inside Scene::Intersect()

			//------------------------------------------------------------------
			// Direct light sampling
			//------------------------------------------------------------------

			DirectLightSampling(renderThread, eyePathIndex,
					sampler->GetSample(eyeVertexSampleOffset + 1),
					sampler->GetSample(eyeVertexSampleOffset + 2),
					sampler->GetSample(eyeVertexSampleOffset + 3),
					sampler->GetSample(eyeVertexSampleOffset + 4),
					sampler->GetSample(eyeVertexSampleOffset + 5),
					eyeVertex);

			//------------------------------------------------------------------
			// Connect vertex path ray with all light path vertices
			//------------------------------------------------------------------

			if (!eyeVertex.bsdf.IsDelta()) {
				for (u_int lightPathIndex = 0; lightPathIndex < lightPaths.size(); ++lightPathIndex) {
					for (u_int lightVertexIndex = 0; lightVertexIndex < lightPaths[lightPathIndex].size(); ++lightVertexIndex) {
						ConnectVertices(renderThread, eyePathIndex, eyeVertex, lightPaths[lightPathIndex][lightVertexIndex],
								sampler->GetSample(eyeVertexSampleOffset + 6));
					}
				}
			}

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			if (!Bounce(renderThread, sampler, eyeVertexSampleOffset + 7,
					&eyeVertex, &eyeRay))
				break;

			++(eyeVertex.depth);
		}
	}
	
	//SLG_LOG("[BiDirState::" << renderThread->threadIndex << "] Generated rays: " << lightSampleResults.size() + eyeSampleRadiance.size());
}

bool BiDirState::ValidResult(BiDirHybridRenderThread *renderThread,
		const Ray *ray, const RayHit *rayHit,
		const float u0, Spectrum *radiance) {
	if (rayHit->Miss())
		return true;
	else {
		BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)renderThread->renderEngine;
		Scene *scene = renderEngine->renderConfig->scene;

		// I have to check if it is an hit over a pass-through point
		BSDF bsdf(false, // true or false, here, doesn't really matter
				*scene, *ray, *rayHit, u0);

		// Check if it is pass-through point
		if (bsdf.IsPassThrough()) {
			// It is a pass-through material, continue to trace the ray. I do
			// this on the CPU.

			Ray newRay(*ray);
			newRay.mint = rayHit->t + MachineEpsilon::E(rayHit->t);
			RayHit newRayHit;
			Spectrum connectionThroughput;
			if (scene->Intersect(renderThread->device, false, // true or false, here, doesn't really matter
					true, u0, &newRay, &newRayHit, &bsdf, &connectionThroughput)) {
				// Something was hit
				return false;
			} else {
				*radiance *= connectionThroughput;
				return true;
			}
		} else
			return false;
	}
}

double BiDirState::CollectResults(HybridRenderThread *renderThread) {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	
	vector<SampleResult> validSampleResults;

	// Elaborate the RayHit results for each eye paths
	SampleResult eyeSampleResult;
	eyeSampleResult.type = PER_PIXEL_NORMALIZED;
	u_int currentLightSampleResultsIndex = 0;
	for (u_int eyePathIndex = 0; eyePathIndex < renderEngine->eyePathCount; ++eyePathIndex) {
		// For each eye path, elaborate the RayHit results for eye to light path vertex connections
		for (u_int i = 0; i < eyeSampleResults[eyePathIndex].lightPathVertexConnections; ++i) {
			const Ray *ray;
			const RayHit *rayHit;
			thread->PopRay(&ray, &rayHit);

			if (ValidResult(thread, ray, rayHit, lightSampleValue[currentLightSampleResultsIndex],
					&lightSampleResults[currentLightSampleResultsIndex].radiance))
				validSampleResults.push_back(lightSampleResults[currentLightSampleResultsIndex]);

			++currentLightSampleResultsIndex;
		}

		eyeSampleResult.alpha = eyeSampleResults[eyePathIndex].alpha;
		eyeSampleResult.screenX = eyeSampleResults[eyePathIndex].screenX;
		eyeSampleResult.screenY = eyeSampleResults[eyePathIndex].screenY;
		eyeSampleResult.radiance = eyeSampleResults[eyePathIndex].radiance;
		for (u_int i = 0; i < eyeSampleResults[eyePathIndex].sampleRadiance.size(); ++i) {
			const Ray *ray;
			const RayHit *rayHit;
			thread->PopRay(&ray, &rayHit);

			if (ValidResult(thread, ray, rayHit, eyeSampleResults[eyePathIndex].sampleValue[i],
					&eyeSampleResults[eyePathIndex].sampleRadiance[i]))
				eyeSampleResult.radiance += eyeSampleResults[eyePathIndex].sampleRadiance[i];
		}
		validSampleResults.push_back(eyeSampleResult);
	}

	sampler->NextSample(validSampleResults);

	// This is a very raw (and somewhat boosted) approximation
	return renderEngine->eyePathCount * renderEngine->lightPathCount;
}
