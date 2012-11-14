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

#include "renderconfig.h"
#include "lightcpu/lightcpu.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/sampler/sampler.h"

//------------------------------------------------------------------------------
// LightCPU RenderThread
//------------------------------------------------------------------------------

LightCPURenderThread::LightCPURenderThread(LightCPURenderEngine *engine,
		const u_int index, const u_int seedVal) :
		CPURenderThread(engine, index, seedVal, true, true) {
}

void LightCPURenderThread::ConnectToEye(const float u0,
		const BSDF &bsdf, const Point &lensPoint, const Spectrum &flux,
		vector<SampleResult> &sampleResults) {
	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Vector eyeDir(bsdf.hitPoint - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	BSDFEvent event;
	Spectrum bsdfEval = bsdf.Evaluate(-eyeDir, &event);

	if (!bsdfEval.Black()) {
		Ray eyeRay(lensPoint, eyeDir);
		eyeRay.maxt = eyeDistance - MachineEpsilon::E(eyeDistance);

		float scrX, scrY;
		if (scene->camera->GetSamplePosition(lensPoint, eyeDir, eyeDistance, &scrX, &scrY)) {
			RayHit eyeRayHit;
			BSDF bsdfConn;
			Spectrum connectionThroughput;
			++raysCount;
			if (!scene->Intersect(true, true, u0, &eyeRay, &eyeRayHit, &bsdfConn, &connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				const float cosToCamera = Dot(bsdf.shadeN, -eyeDir);
				const float cosAtCamera = Dot(scene->camera->GetDir(), eyeDir);

				const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
					scene->camera->GetPixelArea());
				const float cameraPdfA = PdfWtoA(cameraPdfW, eyeDistance, cosToCamera);
				const float fluxToRadianceFactor = cameraPdfA;

				const Spectrum radiance = connectionThroughput * flux * fluxToRadianceFactor * bsdfEval;

				AddSampleResult(sampleResults, PER_SCREEN_NORMALIZED, scrX, scrY,
						radiance, 1.f);
			}
		}
	}
}

void LightCPURenderThread::RenderFunc() {
	//SLG_LOG("[LightCPURenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(seed);
	Scene *scene = engine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();

	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, film);
	const unsigned int sampleBootSize = 12;
	const unsigned int sampleStepSize = 6;
	const unsigned int sampleSize = 
		sampleBootSize + // To generate the initial light vertex and trace eye ray
		engine->maxPathDepth * sampleStepSize; // For each light vertex
	sampler->RequestSamples(sampleSize);

	//--------------------------------------------------------------------------
	// Trace light paths
	//--------------------------------------------------------------------------

	vector<SampleResult> sampleResults;
	samplesCount = 0.0;
	while (!boost::this_thread::interruption_requested()) {
		samplesCount += 1.0;
		sampleResults.clear();

		// Select one light source
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(sampler->GetSample(2), &lightPickPdf);

		// Initialize the light path
		float lightEmitPdfW;
		Ray nextEventRay;
		Spectrum lightPathFlux = light->Emit(scene,
			sampler->GetSample(3), sampler->GetSample(4), sampler->GetSample(5), sampler->GetSample(6),
			&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW);
		if (lightPathFlux.Black()) {
			sampler->NextSample(sampleResults);
			continue;
		}
		lightPathFlux /= lightEmitPdfW * lightPickPdf;
		assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

		// Sample a point on the camera lens
		Point lensPoint;
		if (!camera->SampleLens(sampler->GetSample(7), sampler->GetSample(8),
				&lensPoint)) {
			sampler->NextSample(sampleResults);
			continue;
		}

		//----------------------------------------------------------------------
		// I don't try to connect the light vertex directly with the eye
		// because InfiniteLight::Emit() returns a point on the scene bounding
		// sphere. Instead, I trace a ray from the camera like in BiDir.
		// This is also a good why to test the Film Per-Pixel-Normalization and
		// the Per-Screen-Normalization Buffers used by BiDir.
		//----------------------------------------------------------------------

		{
			Ray eyeRay;
			const float screenX = min(sampler->GetSample(0) * filmWidth, (float)(filmWidth - 1));
			const float screenY = min(sampler->GetSample(1) * filmHeight, (float)(filmHeight - 1));
			camera->GenerateRay(screenX, screenY, &eyeRay,
				sampler->GetSample(9), sampler->GetSample(10));

			Spectrum radiance, connectionThroughput;
			RayHit eyeRayHit;
			BSDF bsdf;
			++raysCount;
			const bool somethingWasHit = scene->Intersect(
				false, true, sampler->GetSample(11), &eyeRay, &eyeRayHit, &bsdf, &connectionThroughput);
			if (!somethingWasHit) {
				// Nothing was hit, check infinite lights (including sun)
				radiance += scene->GetEnvLightsRadiance(-eyeRay.d, Point());
			} else {
				// Something was hit, check if it is a light source
				if (bsdf.IsLightSource())
					radiance += bsdf.GetEmittedRadiance(scene);
			}
			radiance *= connectionThroughput;

			// Add a sample even if it is black in order to avoid aliasing problems
			// between sampled pixel and not sampled one (in PER_PIXEL_NORMALIZED buffer)
			AddSampleResult(sampleResults, PER_PIXEL_NORMALIZED,
					screenX, screenY, radiance, somethingWasHit ? 1.f : 0.f);
		}

		//----------------------------------------------------------------------
		// Trace the light path
		//----------------------------------------------------------------------

		int depth = 1;
		while (depth <= engine->maxPathDepth) {
			const unsigned int sampleOffset = sampleBootSize + (depth - 1) * sampleStepSize;

			RayHit nextEventRayHit;
			BSDF bsdf;
			Spectrum connectionThroughput;
			++raysCount;
			if (scene->Intersect(true, true, sampler->GetSample(sampleOffset),
					&nextEventRay, &nextEventRayHit, &bsdf, &connectionThroughput)) {
				// Something was hit
				
				// Check if it is a light source
				if (bsdf.IsLightSource()) {
					// SLG light sources are like black bodies
					break;
				}

				lightPathFlux *= connectionThroughput;

				//--------------------------------------------------------------
				// Try to connect the light path vertex with the eye
				//--------------------------------------------------------------

				ConnectToEye(sampler->GetSample(sampleOffset + 1),
						bsdf, lensPoint, lightPathFlux, sampleResults);

				if (depth >= engine->maxPathDepth)
					break;

				//--------------------------------------------------------------
				// Build the next vertex path ray
				//--------------------------------------------------------------

				float bsdfPdf;
				Vector sampledDir;
				BSDFEvent event;
				float cosSampleDir;
				const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
						sampler->GetSample(sampleOffset + 2),
						sampler->GetSample(sampleOffset + 3),
						sampler->GetSample(sampleOffset + 4),
						&bsdfPdf, &cosSampleDir, &event);
				if (bsdfSample.Black())
					break;

				if (depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = Max(bsdfSample.Filter(), engine->rrImportanceCap);
					if (sampler->GetSample(sampleOffset + 5) < prob)
						bsdfPdf *= prob;
					else
						break;
				}

				lightPathFlux *= bsdfSample * (cosSampleDir / bsdfPdf);
				assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

				nextEventRay = Ray(bsdf.hitPoint, sampledDir);
				++depth;
			} else {
				// Ray lost in space...
				break;
			}
		}

		sampler->NextSample(sampleResults);
	}

	delete sampler;
	delete rndGen;

	//SLG_LOG("[LightCPURenderThread::" << threadIndex << "] Rendering thread halted");
}
