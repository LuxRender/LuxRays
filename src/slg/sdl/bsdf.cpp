/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include "slg/sdl/bsdf.h"
#include "slg/sdl/scene.h"

using namespace luxrays;
using namespace slg;

void BSDF::Init(const bool fixedFromLight, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float passThroughEvent, const Volume *currentVolume) {
	hitPoint.fromLight = fixedFromLight;
	hitPoint.passThroughEvent = passThroughEvent;

	hitPoint.p = ray(rayHit.t);
	hitPoint.fixedDir = -ray.d;

	// Get the triangle
	mesh = scene.objDefs.GetSceneObject(rayHit.meshIndex)->GetExtMesh();

	// Get the material
	material = scene.objDefs.GetSceneObject(rayHit.meshIndex)->GetMaterial();

	// Interpolate face normal
	hitPoint.geometryN = mesh->GetGeometryNormal(rayHit.triangleIndex);
	hitPoint.shadeN = mesh->InterpolateTriNormal(rayHit.triangleIndex, rayHit.b1, rayHit.b2);

	// Set interior and exterior volumes
	hitPoint.intoObject = (Dot(ray.d, hitPoint.geometryN) < 0.f);
	if (hitPoint.intoObject) {
		// From outside to inside the object

		hitPoint.interiorVolume = material->GetInteriorVolume();

		if (!currentVolume)
			hitPoint.exteriorVolume = material->GetExteriorVolume();
		else {
			// if (!material->GetExteriorVolume()) there may be conflict here
			// between the material definition and the currentVolume value.
			// The currentVolume value wins.
			hitPoint.exteriorVolume = currentVolume;
		}
	} else {
		// From inside to outside the object

		if (!currentVolume)
			hitPoint.interiorVolume = material->GetInteriorVolume();
		else {
			// if (!material->GetInteriorVolume()) there may be conflict here
			// between the material definition and the currentVolume value.
			// The currentVolume value wins.
			hitPoint.interiorVolume = currentVolume;
		}

		hitPoint.exteriorVolume = material->GetExteriorVolume();
	}

	// Interpolate color
	hitPoint.color = mesh->InterpolateTriColor(rayHit.triangleIndex, rayHit.b1, rayHit.b2);

	// Interpolate alpha
	hitPoint.alpha = mesh->InterpolateTriAlpha(rayHit.triangleIndex, rayHit.b1, rayHit.b2);

	// Check if it is a light source
	if (material->IsLightSource())
		triangleLightSource = scene.lightDefs.GetLightSourceByMeshIndex(rayHit.meshIndex);
	else
		triangleLightSource = NULL;

	// Interpolate UV coordinates
	hitPoint.uv = mesh->InterpolateTriUV(rayHit.triangleIndex, rayHit.b1, rayHit.b2);

    // Compute geometry differentials
    Vector geometryDpdu, geometryDpdv;
    Normal geometryDndu, geometryDndv;
    const bool hasDPDUV = mesh->GetDifferentials(rayHit.triangleIndex,
            &geometryDpdu, &geometryDpdv,
            &geometryDndu, &geometryDndv);

    if (hasDPDUV) {
        // Initialize shading differentials
        Vector shadeDpdv = Normalize(Cross(hitPoint.shadeN, geometryDpdu));
		Vector shadeDpdu = Cross(shadeDpdv, hitPoint.shadeN);
		shadeDpdv *= (Dot(geometryDpdv, shadeDpdv) > 0.f) ? 1.f : -1.f;

        // Apply bump or normal mapping
        material->Bump(&hitPoint, shadeDpdu, shadeDpdv, geometryDndu, geometryDndu, 1.f);

        // Build the local reference system
        mesh->GetFrame(hitPoint.shadeN, geometryDpdu, geometryDpdv, frame);
    } else {
        // Build the local reference system
        frame.SetFromZ(hitPoint.shadeN);
    }
}

void BSDF::Init(const bool fixedFromLight, const Scene &scene, const luxrays::Ray &ray,
		const Volume &volume, const float t, const float passThroughEvent) {
	hitPoint.fromLight = fixedFromLight;
	hitPoint.passThroughEvent = passThroughEvent;

	hitPoint.p = ray(t);
	hitPoint.fixedDir = -ray.d;

	mesh = NULL;
	material = &volume;

	hitPoint.geometryN = Normal(-ray.d);
	hitPoint.shadeN = hitPoint.geometryN;

	hitPoint.intoObject = true;
	hitPoint.interiorVolume = &volume;
	hitPoint.exteriorVolume = &volume;

	hitPoint.color = Spectrum(1.f);
	hitPoint.alpha = 1.f;

	triangleLightSource = NULL;

	hitPoint.uv = UV(0.f, 0.f);

	// Build the local reference system
	frame.SetFromZ(hitPoint.shadeN);
}

Spectrum BSDF::Evaluate(const Vector &generatedDir,
		BSDFEvent *event, float *directPdfW, float *reversePdfW) const {
	const Vector &eyeDir = hitPoint.fromLight ? generatedDir : hitPoint.fixedDir;
	const Vector &lightDir = hitPoint.fromLight ? hitPoint.fixedDir : generatedDir;

	const float dotLightDirNG = Dot(lightDir, hitPoint.geometryN);
	const float absDotLightDirNG = fabsf(dotLightDirNG);
	const float dotEyeDirNG = Dot(eyeDir, hitPoint.geometryN);
	const float absDotEyeDirNG = fabsf(dotEyeDirNG);

	if ((absDotLightDirNG < DEFAULT_COS_EPSILON_STATIC) ||
			(absDotEyeDirNG < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	const float sideTest = dotEyeDirNG * dotLightDirNG;
	if (((sideTest > 0.f) && !(material->GetEventTypes() & REFLECT)) ||
			((sideTest < 0.f) && !(material->GetEventTypes() & TRANSMIT)))
		return Spectrum();

	const Vector localLightDir = frame.ToLocal(lightDir);
	const Vector localEyeDir = frame.ToLocal(eyeDir);
	const Spectrum result = material->Evaluate(hitPoint, localLightDir, localEyeDir,
			event, directPdfW, reversePdfW);

	// Adjoint BSDF
	if (hitPoint.fromLight)
		return result * (absDotEyeDirNG / absDotLightDirNG);
	else
		return result;
}

Spectrum BSDF::Sample(Vector *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const {
	Vector localFixedDir = frame.ToLocal(hitPoint.fixedDir);
	Vector localSampledDir;

	Spectrum result = material->Sample(hitPoint,
			localFixedDir, &localSampledDir, u0, u1, hitPoint.passThroughEvent,
			pdfW, absCosSampledDir, event, requestedEvent);
	if (result.Black())
		return result;

	*sampledDir = frame.ToWorld(localSampledDir);

	// Adjoint BSDF
	if (hitPoint.fromLight) {
		const float absDotFixedDirNG = AbsDot(hitPoint.fixedDir, hitPoint.geometryN);
		const float absDotSampledDirNG = AbsDot(*sampledDir, hitPoint.geometryN);
		return result * (absDotSampledDirNG / absDotFixedDirNG);
	} else
		return result;
}

void BSDF::Pdf(const Vector &sampledDir, float *directPdfW, float *reversePdfW) const {
	const Vector &eyeDir = hitPoint.fromLight ? sampledDir : hitPoint.fixedDir;
	const Vector &lightDir = hitPoint.fromLight ? hitPoint.fixedDir : sampledDir;
	Vector localLightDir = frame.ToLocal(lightDir);
	Vector localEyeDir = frame.ToLocal(eyeDir);

	material->Pdf(hitPoint, localLightDir, localEyeDir, directPdfW, reversePdfW);
}

Spectrum BSDF::GetPassThroughTransparency() const {
	const Vector localFixedDir = frame.ToLocal(hitPoint.fixedDir);

	return material->GetPassThroughTransparency(hitPoint, localFixedDir, hitPoint.passThroughEvent);
}

Spectrum BSDF::GetEmittedRadiance(float *directPdfA, float *emissionPdfW) const {
	return triangleLightSource ? 
		triangleLightSource->GetRadiance(hitPoint, directPdfA, emissionPdfW) :
		Spectrum();
}
