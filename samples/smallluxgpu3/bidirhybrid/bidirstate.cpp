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

#if !defined(LUXRAYS_DISABLE_OPENCL)

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

BiDirState::BiDirState(BiDirHybridRenderEngine *renderEngine,
		Film *film, RandomGenerator *rndGen) {
	// Setup the sampler
	sampler = renderEngine->renderConfig->AllocSampler(rndGen, film);
	const unsigned int sampleBootSize = 11;
	const unsigned int sampleLightStepSize = 6;
	const unsigned int sampleEyeStepSize = 11;
	const unsigned int sampleSize = 
		sampleBootSize + // To generate the initial light vertex and trace eye ray
		renderEngine->maxLightPathDepth * sampleLightStepSize + // For each light vertex
		renderEngine->maxEyePathDepth * sampleEyeStepSize; // For each eye vertex
	sampler->RequestSamples(sampleSize);
}

BiDirState::~BiDirState() {
	delete sampler;
}

void BiDirState::ConnectVertices(BiDirHybridRenderThread *renderThread,
		const PathVertex &eyeVertex, const PathVertex &lightVertex,
		const float u0) {
	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;

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
			const float lightWeight = eyeBsdfPdfA * (lightVertex.d0 + lightVertex.d1vc * lightBsdfRevPdfW);
			const float eyeWeight = lightBsdfPdfA * (eyeVertex.d0 + eyeVertex.d1vc * eyeBsdfRevPdfW);

			const float misWeight = 1.f / (lightWeight + 1.f + eyeWeight);

			const Spectrum radiance = (misWeight * geometryTerm) * eyeVertex.throughput * eyeBsdfEval *
						lightBsdfEval * lightVertex.throughput;

			// Add the ray to trace and the result
			eyeSampleValue.push_back(u0);
			renderThread->PushRay(eyeRay);
			eyeSampleRadiance.push_back(radiance);
		}
	}
}

void BiDirState::ConnectToEye(BiDirHybridRenderThread *renderThread,
		const unsigned int pixelCount, 
		const PathVertex &lightVertex, const float u0,
		const Point &lensPoint) {
	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;
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
			const float weight = 1.f / (MIS(cameraPdfA / pixelCount) *
				(lightVertex.d0 + lightVertex.d1vc * MIS(bsdfRevPdfW)) + 1.f); // Balance heuristic (MIS)
			const Spectrum radiance = weight * lightVertex.throughput * fluxToRadianceFactor * bsdfEval;

			// Add the ray to trace and the result
			lightSampleValue.push_back(u0);
			renderThread->PushRay(eyeRay);
			AddSampleResult(lightSampleResults, PER_SCREEN_NORMALIZED, scrX, scrY,
					radiance, 1.f);
		}
	}
}

void BiDirState::DirectLightSampling(BiDirHybridRenderThread *renderThread,
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertex &eyeVertex) {
	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;
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
				const float weightLight  = MIS(bsdfPdfW / directLightSamplingPdfW);
				const float weightCamera = MIS(emissionPdfW * cosThetaToLight / (directPdfW * cosThetaAtLight)) *
					(eyeVertex.d0 + eyeVertex.d1vc * MIS(bsdfRevPdfW));
				const float misWeight = 1.f / (weightLight + 1.f + weightCamera);

				const float factor = cosThetaToLight / directLightSamplingPdfW;

				const Spectrum radiance = (misWeight * factor) * eyeVertex.throughput * lightRadiance * bsdfEval;

				// Add the ray to trace and the result
				eyeSampleValue.push_back(u4);
				renderThread->PushRay(shadowRay);
				eyeSampleRadiance.push_back(radiance);
			}
		}
	}
}

void BiDirState::DirectHitFiniteLight(BiDirHybridRenderThread *renderThread,
		const PathVertex &eyeVertex, Spectrum *radiance) const {
	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	float directPdfA, emissionPdfW;
	const Spectrum lightRadiance = eyeVertex.bsdf.GetEmittedRadiance(scene, &directPdfA, &emissionPdfW);
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
	const float misWeight = 1.f / (MIS(directPdfA) * eyeVertex.d0 +
		MIS(emissionPdfW) * eyeVertex.d1vc + 1.f);

	*radiance += eyeVertex.throughput * misWeight * lightRadiance;
}

void BiDirState::DirectHitInfiniteLight(BiDirHybridRenderThread *renderThread,
		const PathVertex &eyeVertex, Spectrum *radiance) const {
	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	float directPdfA, emissionPdfW;
	Spectrum lightRadiance = scene->GetEnvLightsRadiance(eyeVertex.bsdf.fixedDir,
			Point(), &directPdfA, &emissionPdfW);
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
	const float misWeight = 1.f / (MIS(directPdfA) * eyeVertex.d0 +
		MIS(emissionPdfW) * eyeVertex.d1vc + 1.f);

	*radiance += eyeVertex.throughput * misWeight * lightRadiance;
}

void BiDirState::GenerateRays(BiDirHybridRenderThread *renderThread) {
	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = renderThread->threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();
	const unsigned int pixelCount = filmWidth * filmHeight;

	const unsigned int sampleBootSize = 11;
	const unsigned int sampleLightStepSize = 6;
	const unsigned int sampleEyeStepSize = 11;

	lightSampleValue.clear();
	lightSampleResults.clear();
	eyeSampleValue.clear();
	eyeSampleRadiance.clear();

	// Sample a point on the camera lens
	Point lensPoint;
	if (!camera->SampleLens(sampler->GetSample(3), sampler->GetSample(4),
			&lensPoint))
		return;

	vector<PathVertex> lightPathVertices;

	//----------------------------------------------------------------------
	// Trace light path
	//----------------------------------------------------------------------

	// Select one light source
	float lightPickPdf;
	const LightSource *light = scene->SampleAllLights(sampler->GetSample(2), &lightPickPdf);

	// Initialize the light path
	PathVertex lightVertex;
	float lightEmitPdfW, lightDirectPdfW, cosThetaAtLight;
	Ray nextEventRay;
	lightVertex.throughput = light->Emit(scene,
		sampler->GetSample(5), sampler->GetSample(6), sampler->GetSample(7), sampler->GetSample(8),
		&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW, &lightDirectPdfW, &cosThetaAtLight);
	if (!lightVertex.throughput.Black()) {
		lightEmitPdfW *= lightPickPdf;
		lightDirectPdfW *= lightPickPdf;

		lightVertex.throughput /= lightEmitPdfW;
		assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

		// I don't store the light vertex 0 because direct lighting will take
		// care of this kind of paths
		lightVertex.d0 = MIS(lightDirectPdfW / lightEmitPdfW);
		const float usedCosLight = light->IsEnvironmental() ? 1.f : cosThetaAtLight;
		lightVertex.d1vc = MIS(usedCosLight / lightEmitPdfW);

		lightVertex.depth = 1;
		while (lightVertex.depth <= renderEngine->maxLightPathDepth) {
			const unsigned int sampleOffset = sampleBootSize + (lightVertex.depth - 1) * sampleLightStepSize;

			RayHit nextEventRayHit;
			Spectrum connectionThroughput;
			if (scene->Intersect(true, true, sampler->GetSample(sampleOffset),
					&nextEventRay, &nextEventRayHit, &lightVertex.bsdf, &connectionThroughput)) {
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
					lightVertex.d0 *= MIS(nextEventRayHit.t * nextEventRayHit.t);
				const float factor = MIS(1.f / AbsDot(lightVertex.bsdf.shadeN, nextEventRay.d));
				lightVertex.d0 *= factor;
				lightVertex.d1vc *= factor;

				// Store the vertex only if it isn't specular
				if (!lightVertex.bsdf.IsDelta()) {
					lightPathVertices.push_back(lightVertex);

					//------------------------------------------------------
					// Try to connect the light path vertex with the eye
					//------------------------------------------------------
					ConnectToEye(renderThread, pixelCount, lightVertex, sampler->GetSample(sampleOffset + 1), lensPoint);
				}

				if (lightVertex.depth >= renderEngine->maxLightPathDepth)
					break;

				//----------------------------------------------------------
				// Build the next vertex path ray
				//----------------------------------------------------------

				Vector sampledDir;
				BSDFEvent event;
				float bsdfPdfW, cosSampledDir;
				const Spectrum bsdfSample = lightVertex.bsdf.Sample(&sampledDir,
						sampler->GetSample(sampleOffset + 2),
						sampler->GetSample(sampleOffset + 3),
						sampler->GetSample(sampleOffset + 4),
						&bsdfPdfW, &cosSampledDir, &event);
				if (bsdfSample.Black())
					break;

				float bsdfRevPdfW;
				if (event & SPECULAR)
					bsdfRevPdfW = bsdfPdfW;
				else
					lightVertex.bsdf.Pdf(sampledDir, NULL, &bsdfRevPdfW);

				if (lightVertex.depth >= renderEngine->rrDepth) {
					// Russian Roulette
					const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
					if (sampler->GetSample(sampleOffset + 5) < prob) {
						bsdfPdfW *= prob;
						bsdfRevPdfW *= prob;
					} else
						break;
				}

				lightVertex.throughput *= bsdfSample * (cosSampledDir / bsdfPdfW);
				assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

				// New MIS weights
				if (event & SPECULAR) {
					lightVertex.d0 = 0.f;
					lightVertex.d1vc *= (cosSampledDir / bsdfPdfW) * bsdfRevPdfW;
				} else {
					lightVertex.d1vc = (cosSampledDir / bsdfPdfW) * (lightVertex.d1vc *
							bsdfRevPdfW + lightVertex.d0);
					lightVertex.d0 = 1.f / bsdfPdfW;
				}

				nextEventRay = Ray(lightVertex.bsdf.hitPoint, sampledDir);
				++(lightVertex.depth);
			} else {
				// Ray lost in space...
				break;
			}
		}
	}

	//----------------------------------------------------------------------
	// Trace eye path
	//----------------------------------------------------------------------

	PathVertex eyeVertex;
	eyeAlpha = 1.f;
	eyeRadiance = Spectrum();

	Ray eyeRay;
	eyeScreenX = min(sampler->GetSample(0) * filmWidth, (float)(filmWidth - 1));
	eyeScreenY = min(sampler->GetSample(1) * filmHeight, (float)(filmHeight - 1));
	camera->GenerateRay(eyeScreenX, eyeScreenY, &eyeRay,
		sampler->GetSample(9), sampler->GetSample(10));

	eyeVertex.bsdf.fixedDir = -eyeRay.d;
	eyeVertex.throughput = Spectrum(1.f, 1.f, 1.f);
	const float cosAtCamera = Dot(scene->camera->GetDir(), eyeRay.d);
	const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
		scene->camera->GetPixelArea() * pixelCount);
	eyeVertex.d0 = MIS(1.f / cameraPdfW);
	eyeVertex.d1vc = 0.f;

	eyeVertex.depth = 1;
	while (eyeVertex.depth <= renderEngine->maxEyePathDepth) {
		const unsigned int sampleOffset = sampleBootSize + renderEngine->maxLightPathDepth * sampleLightStepSize +
			(eyeVertex.depth - 1) * sampleEyeStepSize;

		RayHit eyeRayHit;
		Spectrum connectionThroughput;
		if (!scene->Intersect(false, true, sampler->GetSample(sampleOffset), &eyeRay,
				&eyeRayHit, &eyeVertex.bsdf, &connectionThroughput)) {
			// Nothing was hit, look for infinitelight

			// This is a trick, you can not have a BSDF of something that has
			// not been hit. DirectHitInfiniteLight must be aware of this.
			eyeVertex.bsdf.fixedDir = -eyeRay.d;
			eyeVertex.throughput *= connectionThroughput;

			DirectHitInfiniteLight(renderThread, eyeVertex, &eyeRadiance);

			if (eyeVertex.depth == 1)
				eyeAlpha = 0.f;
			break;
		}
		eyeVertex.throughput *= connectionThroughput;

		// Something was hit

		// Update MIS constants
		eyeVertex.d0 *= MIS(eyeRayHit.t * eyeRayHit.t);
		const float factor = MIS(1.f / AbsDot(eyeVertex.bsdf.shadeN, eyeVertex.bsdf.fixedDir));
		eyeVertex.d0 *= factor;
		eyeVertex.d1vc *= factor;

		// Check if it is a light source
		if (eyeVertex.bsdf.IsLightSource()) {
			DirectHitFiniteLight(renderThread, eyeVertex, &eyeRadiance);

			// SLG light sources are like black bodies
			break;
		}

		// Note: pass-through check is done inside SceneIntersect()

		//------------------------------------------------------------------
		// Direct light sampling
		//------------------------------------------------------------------

		DirectLightSampling(renderThread,
				sampler->GetSample(sampleOffset + 1),
				sampler->GetSample(sampleOffset + 2),
				sampler->GetSample(sampleOffset + 3),
				sampler->GetSample(sampleOffset + 4),
				sampler->GetSample(sampleOffset + 5),
				eyeVertex);

		//------------------------------------------------------------------
		// Connect vertex path ray with all light path vertices
		//------------------------------------------------------------------

		if (!eyeVertex.bsdf.IsDelta()) {
			for (vector<PathVertex>::const_iterator lightPathVertex = lightPathVertices.begin();
					lightPathVertex < lightPathVertices.end(); ++lightPathVertex)
				ConnectVertices(renderThread, eyeVertex, *lightPathVertex,
						sampler->GetSample(sampleOffset + 6));
		}

		//------------------------------------------------------------------
		// Build the next vertex path ray
		//------------------------------------------------------------------

		Vector sampledDir;
		BSDFEvent event;
		float cosSampledDir, bsdfPdfW;
		const Spectrum bsdfSample = eyeVertex.bsdf.Sample(&sampledDir,
				sampler->GetSample(sampleOffset + 7),
				sampler->GetSample(sampleOffset + 8),
				sampler->GetSample(sampleOffset + 9),
				&bsdfPdfW, &cosSampledDir, &event);
		if (bsdfSample.Black())
			break;

		float bsdfRevPdfW;
		if (event & SPECULAR)
			bsdfRevPdfW = bsdfPdfW;
		else
			eyeVertex.bsdf.Pdf(sampledDir, NULL, &bsdfRevPdfW);

		if (eyeVertex.depth >= renderEngine->rrDepth) {
			// Russian Roulette
			const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
			if (prob > sampler->GetSample(sampleOffset + 10)) {
				bsdfPdfW *= prob;
				bsdfRevPdfW *= prob;
			} else
				break;
		}

		eyeVertex.throughput *= bsdfSample * (cosSampledDir / bsdfPdfW);
		assert (!eyeVertex.throughput.IsNaN() && !eyeVertex.throughput.IsInf());

		// New MIS weights
		if (event & SPECULAR) {
			eyeVertex.d0 = 0.f;
			eyeVertex.d1vc *= MIS(cosSampledDir / bsdfPdfW) * MIS(bsdfRevPdfW);
		} else {
			eyeVertex.d1vc = MIS(cosSampledDir / bsdfPdfW) * (eyeVertex.d1vc *
					MIS(bsdfRevPdfW) + eyeVertex.d0);
			eyeVertex.d0 = MIS(1.f / bsdfPdfW);
		}

		eyeRay = Ray(eyeVertex.bsdf.hitPoint, sampledDir);
		++(eyeVertex.depth);
	}
	
	//SLG_LOG("[BiDirState::" << renderThread->threadIndex << "] Generated rays: " << lightSampleResults.size() + eyeSampleRadiance.size());
}

static bool ValidResult(Scene *scene, const Ray *ray, const RayHit *rayHit,
		const float u0, Spectrum *radiance) {
	if (rayHit->Miss())
		return true;
	else {
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
			if (scene->Intersect(false, // true or false, here, doesn't really matter
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

void BiDirState::CollectResults(BiDirHybridRenderThread *renderThread) {
	if (lightSampleResults.size() + eyeSampleRadiance.size() == 0)
		sampler->NextSample(lightSampleResults);

	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	vector<SampleResult> validSampleResults;

	// Elaborate the RayHit results for light path
	for (u_int i = 0; i < lightSampleResults.size(); ++i) {
		const Ray *ray;
		const RayHit *rayHit;
		renderThread->PopRay(&ray, &rayHit);

		if (ValidResult(scene, ray, rayHit, lightSampleValue[i], &lightSampleResults[i].radiance))
			validSampleResults.push_back(lightSampleResults[i]);
	}
	
	// Elaborate the RayHit results for eye path
	SampleResult eyeSampleResult;
	eyeSampleResult.type = PER_PIXEL_NORMALIZED;
	eyeSampleResult.alpha = eyeAlpha;
	eyeSampleResult.screenX = eyeScreenX;
	eyeSampleResult.screenY = eyeScreenY;
	eyeSampleResult.radiance = eyeRadiance;
	for (u_int i = 0; i < eyeSampleRadiance.size(); ++i) {
		const Ray *ray;
		const RayHit *rayHit;
		renderThread->PopRay(&ray, &rayHit);

		if (ValidResult(scene, ray, rayHit, eyeSampleValue[i], &eyeSampleRadiance[i]))
			eyeSampleResult.radiance += eyeSampleRadiance[i];
	}
	validSampleResults.push_back(eyeSampleResult);

	sampler->NextSample(validSampleResults);
}

#endif
