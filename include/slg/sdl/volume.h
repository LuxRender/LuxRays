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

#ifndef _SLG_VOLUME_H
#define	_SLG_VOLUME_H

#include "luxrays/luxrays.h"
#include "slg/sdl/material.h"
#include "slg/sdl/texture.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/sdl/volume_types.cl"
}

class BSDF;
	
class Volume : public Material {
public:
	Volume(const Texture *ior, const Texture *emission) : Material(NULL, NULL),
			iorTex(ior), volumeEmissionTex(emission), volumeLightID(0), priority(0) { }
	virtual ~Volume() { }

	virtual std::string GetName() const { return "volume-" + boost::lexical_cast<std::string>(this); }

	void SetVolumeLightID(const u_int id) { volumeLightID = id; }
	u_int GetVolumeLightID() const { return volumeLightID; }
	void SetPriority(const int p) { priority = p; }
	int GetPriority() const { return priority; }

	const Texture *GetIORTexture() const { return iorTex; }
	const Texture *GetVolumeEmissionTexture() const { return volumeEmissionTex; }

	float GetIOR(const HitPoint &hitPoint) const { return iorTex->GetFloatValue(hitPoint); }

	// Returns the ray t value of the scatter event. If (t <= 0.0) there was
	// no scattering. In any case, it applies transmittance to connectionThroughput
	// too.
	virtual float Scatter(const luxrays::Ray &ray, const float u, const bool scatteredStart,
		luxrays::Spectrum *connectionThroughput, luxrays::Spectrum *connectionEmission) const = 0;

	virtual luxrays::Properties ToProperties() const;

	friend class SchlickScatter;

protected:
	virtual luxrays::Spectrum SigmaA(const HitPoint &hitPoint) const = 0;
	virtual luxrays::Spectrum SigmaS(const HitPoint &hitPoint) const = 0;
	virtual luxrays::Spectrum SigmaT(const HitPoint &hitPoint) const {
		return SigmaA(hitPoint) + SigmaS(hitPoint);
	}

	const Texture *iorTex;
	// This is a different kind of emission texture from the one in
	// Material class (i.e. is not sampled by direct light).
	const Texture *volumeEmissionTex;
	u_int volumeLightID;
	int priority;
};

// An utility class
class SchlickScatter {
public:
	SchlickScatter(const Volume *volume, const Texture *g);

	luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const;
	void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	const Volume *volume;
	const Texture *g;
};

// A class used to store volume related information on the on going path
#define PATHVOLUMEINFO_SIZE 8

class PathVolumeInfo {
public:
	PathVolumeInfo();

	const Volume *GetCurrentVolume() const { return currentVolume; }
	const u_int GetListSize() const { return volumeListSize; }

	void AddVolume(const Volume *v);
	void RemoveVolume(const Volume *v);

	void SetScatteredStart(const bool v) { scatteredStart = v; }
	bool IsScatteredStart() const { return scatteredStart; }
	
	void Update(const BSDFEvent eventType, const BSDF &bsdf);
	bool ContinueToTrace(const BSDF &bsdf) const;

private:
	static bool CompareVolumePriorities(const Volume *vol1, const Volume *vol2);

	const Volume *currentVolume;
	// Using a fixed array here mostly to have the same code as the OpenCL implementation
	const Volume *volumeList[PATHVOLUMEINFO_SIZE];
	u_int volumeListSize;
	
	bool scatteredStart;
};

//------------------------------------------------------------------------------
// ClearVolume
//------------------------------------------------------------------------------

class ClearVolume : public Volume {
public:
	ClearVolume(const Texture *iorTex, const Texture *emiTex, const Texture *a);

	virtual float Scatter(const luxrays::Ray &ray, const float u, const bool scatteredStart,
		luxrays::Spectrum *connectionThroughput, luxrays::Spectrum *connectionEmission) const;

	// Material interface

	virtual MaterialType GetType() const { return CLEAR_VOL; }
	virtual BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT; };

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const;
	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Texture *GetSigmaA() const { return sigmaA; }

protected:
	virtual luxrays::Spectrum SigmaA(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum SigmaS(const HitPoint &hitPoint) const;

private:
	const Texture *sigmaA;
};

//------------------------------------------------------------------------------
// HomogeneousVolume
//------------------------------------------------------------------------------

class HomogeneousVolume : public Volume {
public:
	HomogeneousVolume(const Texture *iorTex, const Texture *emiTex,
			const Texture *a, const Texture *s,
			const Texture *g, const bool multiScattering);

	virtual float Scatter(const luxrays::Ray &ray, const float u, const bool scatteredStart,
		luxrays::Spectrum *connectionThroughput, luxrays::Spectrum *connectionEmission) const;

	// Material interface

	virtual MaterialType GetType() const { return HOMOGENEOUS_VOL; }
	virtual BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT; };

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const;
	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

protected:
	virtual luxrays::Spectrum SigmaA(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum SigmaS(const HitPoint &hitPoint) const;

private:
	const Texture *sigmaA, *sigmaS;
	SchlickScatter schlickScatter;
	const bool multiScattering;
};

//------------------------------------------------------------------------------
// HeterogeneousVolume
//------------------------------------------------------------------------------

class HeterogeneousVolume : public Volume {
public:
	HeterogeneousVolume(const Texture *iorTex, const Texture *emiTex,
			const Texture *a, const Texture *s,
			const Texture *g, const float stepSize, const u_int maxStepsCount,
			const bool multiScattering);

	virtual float Scatter(const luxrays::Ray &ray, const float u, const bool scatteredStart,
		luxrays::Spectrum *connectionThroughput, luxrays::Spectrum *connectionEmission) const;

	// Material interface

	virtual MaterialType GetType() const { return HETEROGENEOUS_VOL; }
	virtual BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT; };

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const;
	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

protected:
	virtual luxrays::Spectrum SigmaA(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum SigmaS(const HitPoint &hitPoint) const;

private:
	const Texture *sigmaA, *sigmaS;
	SchlickScatter schlickScatter;
	float stepSize;
	u_int maxStepsCount;
	const bool multiScattering;
};

}

#endif	/* _SLG_VOLUME_H */
