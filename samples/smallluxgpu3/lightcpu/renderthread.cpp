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
#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/sdl/bsdf.h"

// TODO: DOF support

//------------------------------------------------------------------------------
// LightCPU RenderThread
//------------------------------------------------------------------------------

static void ConnectToEye(const Scene *scene, Film *film, const float u0,
		Vector eyeDir, const float eyeDistance, const Point &lensPoint,
		const Normal &shadeN, const Spectrum &bsdfEval,
		const Spectrum &flux) {
	if (!bsdfEval.Black()) {
		Ray eyeRay(lensPoint, eyeDir);
		eyeRay.maxt = eyeDistance - MachineEpsilon::E(eyeDistance);

		float scrX, scrY;
		if (scene->camera->GetSamplePosition(lensPoint, eyeDir, eyeDistance, &scrX, &scrY)) {
			for (;;) {
				RayHit eyeRayHit;
				if (!scene->dataSet->Intersect(&eyeRay, &eyeRayHit)) {
					// Nothing was hit, the light path vertex is visible

					const float cosToCamera = Dot(shadeN, -eyeDir);
					const float cosAtCamera = Dot(scene->camera->GetDir(), eyeDir);

					const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
						scene->camera->GetPixelArea());
					const float cameraPdfA = PdfWtoA(cameraPdfW, eyeDistance, cosToCamera);
					const float fluxToRadianceFactor = cameraPdfA;

					film->SplatFiltered(scrX, scrY, flux * fluxToRadianceFactor * bsdfEval);
					break;
				} else {
					// Check if it is a pass through point
					BSDF bsdf(true, *scene, eyeRay, eyeRayHit, u0);

					// Check if it is pass-through point
					if (bsdf.IsPassThrough()) {
						// It is a pass-through material, continue to trace the ray
						eyeRay.mint = eyeRayHit.t + MachineEpsilon::E(eyeRayHit.t);
						eyeRay.maxt = std::numeric_limits<float>::infinity();
						continue;
					}
					break;
				}
			}
		}
	}
}

static void ConnectToEye(const Scene *scene, Film *film, const float u0,
		const BSDF &bsdf, const Point &lensPoint, const Vector &lightDir, const Spectrum &flux) {
	Vector eyeDir(bsdf.hitPoint - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	BSDFEvent event;
	Spectrum bsdfEval = bsdf.Evaluate(lightDir, -eyeDir, &event);

	ConnectToEye(scene, film, u0, eyeDir, eyeDistance, lensPoint, bsdf.shadeN, bsdfEval, flux);
}

static Spectrum DirectHitLightSampling(const Scene *scene,
		const Vector &eyeDir, const float distance,
		const BSDF &bsdf) {
	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(scene,
		eyeDir, &directPdfA);

	if (!emittedRadiance.Black()) {
		return emittedRadiance;
	} else
		return Spectrum();
}

static Spectrum DirectHitInfiniteLight(const Scene *scene, const Vector &eyeDir) {
	if (!scene->infiniteLight)
		return Spectrum();

	float directPdfW;
	Spectrum lightRadiance = scene->infiniteLight->GetRadiance(
			scene, -eyeDir, Point(), &directPdfW);
	if (lightRadiance.Black())
		return Spectrum();

	return lightRadiance;
}

void LightCPURenderEngine::RenderThreadFuncImpl(CPURenderThread *renderThread) {
	//SLG_LOG("[LightCPURenderThread::" << renderThread->threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	LightCPURenderEngine *renderEngine = (LightCPURenderEngine *)renderThread->renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(renderThread->threadIndex + renderThread->seed);
	Scene *scene = renderEngine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *lightFilm = renderThread->threadFilmPSN;
	Film *eyeFilm = renderThread->threadFilmPPN;
	const unsigned int filmWidth = eyeFilm->GetWidth();
	const unsigned int filmHeight = eyeFilm->GetHeight();

	//--------------------------------------------------------------------------
	// Trace light paths
	//--------------------------------------------------------------------------

	while (!boost::this_thread::interruption_requested()) {
		lightFilm->AddSampleCount(1);

		// Select one light source
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lightPickPdf);

		// Initialize the light path
		float lightEmitPdf;
		Ray nextEventRay;
		Normal lightN;
		Spectrum lightPathFlux = light->Emit(scene,
			rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
			&nextEventRay.o, &nextEventRay.d, &lightN, &lightEmitPdf);
		if (lightPathFlux.Black())
			continue;
		lightPathFlux /= lightEmitPdf * lightPickPdf;
		assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

		// Sample a point on the camera lens
		Point lensPoint;
		if (!scene->camera->SampleLens(rndGen->floatValue(), rndGen->floatValue(),
				rndGen->floatValue(), &lensPoint))
			continue;

		//----------------------------------------------------------------------
		// I don't try to connect the light vertex directly with the eye
		// because InfiniteLight::Emit() returns a point on the scene bounding
		// sphere. Instead, I trace a ray from the camera like in BiDir.
		// This is also a good why to test the Per-Pixel-Normalization and
		// the Per-Screen-Normalization Buffers used by BiDir.
		//----------------------------------------------------------------------

		{
			Ray eyeRay;
			const float screenX = min(rndGen->floatValue() * filmWidth, (float)(filmWidth - 1));
			const float screenY = min(rndGen->floatValue() * filmHeight, (float)(filmHeight - 1));
			camera->GenerateRay(screenX, screenY, &eyeRay,
				rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue());

			Spectrum radiance;
			RayHit eyeRayHit;
			if (!scene->dataSet->Intersect(&eyeRay, &eyeRayHit))
				radiance = DirectHitInfiniteLight(scene, eyeRay.d);
			else {
				// Something was hit
				BSDF bsdf(false, *scene, eyeRay, eyeRayHit, rndGen->floatValue());

				// Check if it is a light source
				if (bsdf.IsLightSource())
					radiance = DirectHitLightSampling(scene, -eyeRay.d, eyeRayHit.t, bsdf);
			}

			if (!radiance.Black())
				eyeFilm->SplatFiltered(screenX, screenY, radiance);
		}
		
		//----------------------------------------------------------------------
		// Trace the light path
		//----------------------------------------------------------------------
		int depth = 1;
		while (depth <= renderEngine->maxPathDepth) {
			RayHit nextEventRayHit;
			if (scene->dataSet->Intersect(&nextEventRay, &nextEventRayHit)) {
				// Something was hit
				BSDF bsdf(true, *scene, nextEventRay, nextEventRayHit, rndGen->floatValue());

				// Check if it is a light source
				if (bsdf.IsLightSource()) {
					// SLG light sources are like black bodies
					break;
				}

				// Check if it is pass-through point
				if (bsdf.IsPassThrough()) {
					// It is a pass-through material, continue to trace the ray
					nextEventRay.mint = nextEventRayHit.t + MachineEpsilon::E(nextEventRayHit.t);
					nextEventRay.maxt = std::numeric_limits<float>::infinity();
					continue;
				}

				//--------------------------------------------------------------
				// Try to connect the light path vertex with the eye
				//--------------------------------------------------------------

				ConnectToEye(scene, lightFilm, rndGen->floatValue(),
						bsdf, lensPoint, -nextEventRay.d, lightPathFlux);

				if (depth >= renderEngine->maxPathDepth)
					break;
				
				//--------------------------------------------------------------
				// Build the next vertex path ray
				//--------------------------------------------------------------

				float bsdfPdf;
				Vector sampledDir;
				BSDFEvent event;
				const Spectrum bsdfSample = bsdf.Sample(-nextEventRay.d, &sampledDir,
						rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
						&bsdfPdf, &event);
				if ((bsdfPdf <= 0.f) || bsdfSample.Black())
					break;

				if (depth >= renderEngine->rrDepth) {
					// Russian Roulette
					const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
					if (prob > rndGen->floatValue())
						bsdfPdf *= prob;
					else
						break;
				}

				lightPathFlux *= bsdfSample / bsdfPdf;
				assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

				nextEventRay = Ray(bsdf.hitPoint, sampledDir);
				++depth;
			} else {
				// Ray lost in space...
				break;
			}
		}
	}

	//SLG_LOG("[LightCPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
}
