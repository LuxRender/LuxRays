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
		const Normal &shadeN, const Spectrum bsdfEval,
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
		const BSDF &bsdf, const Point &lensPoint, const Vector &lightDir, const Spectrum flux) {
	Vector eyeDir(bsdf.hitPoint - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	BSDFEvent event;
	Spectrum bsdfEval = bsdf.Evaluate(lightDir, -eyeDir, &event);

	ConnectToEye(scene, film, u0, eyeDir, eyeDistance, lensPoint, bsdf.shadeN, bsdfEval, flux);
}

void LightCPURenderEngine::RenderThreadFuncImpl(CPURenderThread *renderThread) {
	//SLG_LOG("[LightCPURenderThread::" << renderThread->threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	LightCPURenderEngine *renderEngine = (LightCPURenderEngine *)renderThread->renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(renderThread->threadIndex + renderThread->seed);
	Scene *scene = renderEngine->renderConfig->scene;
	Film *film = renderThread->threadFilmPSN;

	//--------------------------------------------------------------------------
	// Trace light paths
	//--------------------------------------------------------------------------

	while (!boost::this_thread::interruption_requested()) {
		film->AddSampleCount(1);

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

		//----------------------------------------------------------------------
		// Try to connect the light vertex directly with the eye
		//----------------------------------------------------------------------

		Point lensPoint;
		if (!scene->camera->SampleLens(rndGen->floatValue(), rndGen->floatValue(),
				rndGen->floatValue(), &lensPoint))
			continue;

		{
			Vector eyeDir(nextEventRay.o - lensPoint);
			if (Dot(-eyeDir, lightN) > 0.f) {
				const float eyeDistance = eyeDir.Length();
				eyeDir /= eyeDistance;

				float emissionPdfW;
				Spectrum lightRadiance = light->GetRadiance(scene, -eyeDir, nextEventRay.o, NULL, &emissionPdfW);
				lightRadiance /= emissionPdfW;

				ConnectToEye(scene, film, rndGen->floatValue(), eyeDir, eyeDistance, lensPoint,
						lightN, Spectrum(1.f, 1.f, 1.f), lightRadiance);
			}
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

				ConnectToEye(scene, film, rndGen->floatValue(),
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

				lightPathFlux *= AbsDot(bsdf.shadeN, sampledDir) * bsdfSample / bsdfPdf;
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
