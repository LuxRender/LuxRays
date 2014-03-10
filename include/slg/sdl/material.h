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

#ifndef _SLG_MATERIAL_H
#define	_SLG_MATERIAL_H

#include <vector>
#include <set>

#include <boost/lexical_cast.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/color/color.h"
#include "luxrays/utils/mc.h"
#include "luxrays/core/geometry/point.h"
#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/sdl/bsdfevents.h"
#include "slg/sdl/texture.h"
#include "slg/sdl/hitpoint.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/sdl/material_types.cl"
}

class Scene;

typedef enum {
	MATTE, MIRROR, GLASS, ARCHGLASS, MIX, NULLMAT, MATTETRANSLUCENT,
	GLOSSY2, METAL2, ROUGHGLASS, VELVET, CLOTH, CARPAINT,

	// Volumes
	HOMOGENEOUS_VOL, CLEAR_VOL, HETEROGENEOUS_VOL,

	// The following types are used (in PATHOCL CompiledScene class) only to
	// recognize the usage of some specific material option
	GLOSSY2_ANISOTROPIC, GLOSSY2_ABSORPTION, GLOSSY2_INDEX, GLOSSY2_MULTIBOUNCE,
	METAL2_ANISOTROPIC, ROUGHGLASS_ANISOTROPIC
} MaterialType;

class Material {
public:
	Material(const Texture *emitted, const Texture *bump) :
		matID(0), lightID(0), samples(-1), emittedSamples(-1),
		emittedGain(1.f), emittedPower(0.f), emittedEfficency(0.f),
		emittedTex(emitted), bumpTex(bump), bumpSampleDistance(.001f),
		emissionMap(NULL), emissionFunc(NULL),
		interiorVolume(NULL), exteriorVolume(NULL),
		isVisibleIndirectDiffuse(true), isVisibleIndirectGlossy(true), isVisibleIndirectSpecular(true) {
		UpdateEmittedFactor();
	}
	virtual ~Material() {
		delete emissionFunc;
	}

	virtual std::string GetName() const { return "material-" + boost::lexical_cast<std::string>(this); }
	void SetLightID(const u_int id) { lightID = id; }
	u_int GetLightID() const { return lightID; }
	void SetID(const u_int id) { matID = id; }
	u_int GetID() const { return matID; }
	void SetEmittedGain(const luxrays::Spectrum &v) { emittedGain = v; UpdateEmittedFactor(); }
	luxrays::Spectrum GetEmittedGain() const { return emittedGain; }
	void SetEmittedPower(const float v) { emittedPower = v; UpdateEmittedFactor(); }
	float GetEmittedPower() const { return emittedPower; }
	void SetEmittedEfficency(const float v) { emittedEfficency = v; UpdateEmittedFactor(); }
	float GetEmittedEfficency() const { return emittedEfficency; }
	const luxrays::Spectrum &GetEmittedFactor() const { return emittedFactor; }
	bool IsUsingPrimitiveArea() const { return usePrimitiveArea; }

	virtual MaterialType GetType() const = 0;
	virtual BSDFEvent GetEventTypes() const = 0;

	virtual bool IsLightSource() const {
		return (emittedTex != NULL);
	}
	virtual bool HasBumpTex() const { 
		return (bumpTex != NULL);
	}

	void SetIndirectDiffuseVisibility(const bool visible) { isVisibleIndirectDiffuse = visible; }
	bool IsVisibleIndirectDiffuse() const { return isVisibleIndirectDiffuse; }
	void SetIndirectGlossyVisibility(const bool visible) { isVisibleIndirectGlossy = visible; }
	bool IsVisibleIndirectGlossy() const { return isVisibleIndirectGlossy; }
	void SetIndirectSpecularVisibility(const bool visible) { isVisibleIndirectSpecular = visible; }
	bool IsVisibleIndirectSpecular() const { return isVisibleIndirectSpecular; }

    void SetBumpSampleDistance(const float dist) { bumpSampleDistance = dist; }
    float GetBumpSampleDistance() const { return bumpSampleDistance; }

	virtual bool IsDelta() const { return false; }
	virtual bool IsPassThrough() const { return false; }
	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent) const {
		return luxrays::Spectrum(0.f);
	}

	virtual luxrays::Spectrum GetEmittedRadiance(const HitPoint &hitPoint,
		const float oneOverPrimitiveArea) const;
	virtual float GetEmittedRadianceY() const {
		if (emittedTex)
			return emittedTex->Y();
		else
			return 0.f;
	}

	const void SetSamples(const int sampleCount) { samples = sampleCount; }
	const int GetSamples() const { return samples; }
	const void SetEmittedSamples(const int sampleCount) { emittedSamples = sampleCount; }
	const int GetEmittedSamples() const { return emittedSamples; }
	const Texture *GetEmitTexture() const { return emittedTex; }
	const Texture *GetBumpTexture() const { return bumpTex; }
	void SetEmissionMap(const ImageMap *map);
	const ImageMap *GetEmissionMap() const { return emissionMap; }
	const SampleableSphericalFunction *GetEmissionFunc() const { return emissionFunc; }

	// MixMaterial can have multiple volumes assigned and need the passThroughEvent
	// information to be able to return the correct volume
	void SetInteriorVolume(const Volume *vol) { interiorVolume = vol; }
	virtual const Volume *GetInteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const { return interiorVolume; }
	void SetExteriorVolume(const Volume *vol) { exteriorVolume = vol; }
	virtual const Volume *GetExteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const { return exteriorVolume; }	
	
    virtual void Bump(HitPoint *hitPoint,
        const luxrays::Vector &dpdu, const luxrays::Vector &dpdv,
        const luxrays::Normal &dndu, const luxrays::Normal &dndv,
        const float weight) const;

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const = 0;

	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent = ALL) const = 0;

	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const = 0;

	// Update any reference to oldMat with newMat (mostly used for updating Mix material)
	virtual void UpdateMaterialReferences(const Material *oldMat, const Material *newMat) { }
	// Return true if the material is referencing the specified material
	virtual bool IsReferencing(const Material *mat) const { return (this == mat); }
	virtual void AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const {
		referencedMats.insert(this);
		if (interiorVolume)
			referencedMats.insert((const Material *)interiorVolume);
		if (exteriorVolume)
			referencedMats.insert((const Material *)exteriorVolume);
	}
	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		if (emittedTex)
			emittedTex->AddReferencedTextures(referencedTexs);
		if (bumpTex)
			bumpTex->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		if (emissionMap)
			referencedImgMaps.insert(emissionMap);
	}
	// Update any reference to oldTex with newTex
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (emittedTex == oldTex)
			emittedTex = newTex;
		if (bumpTex == oldTex)
			bumpTex = newTex;
	}
	
	virtual luxrays::Properties ToProperties() const;

protected:
	void UpdateEmittedFactor();

	u_int matID, lightID;

	int samples, emittedSamples;
	luxrays::Spectrum emittedGain, emittedFactor;
	float emittedPower, emittedEfficency;

	const Texture *emittedTex;
	const Texture *bumpTex;
    float bumpSampleDistance;

	const ImageMap *emissionMap;
	SampleableSphericalFunction *emissionFunc;

	const Volume *interiorVolume, *exteriorVolume;

	bool isVisibleIndirectDiffuse, isVisibleIndirectGlossy, isVisibleIndirectSpecular, usePrimitiveArea;
};

//------------------------------------------------------------------------------
// MaterialDefinitions
//------------------------------------------------------------------------------

class MaterialDefinitions {
public:
	MaterialDefinitions();
	~MaterialDefinitions();

	bool IsMaterialDefined(const std::string &name) const {
		return (matsByName.count(name) > 0);
	}
	void DefineMaterial(const std::string &name, Material *m);

	void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	Material *GetMaterial(const std::string &name);
	Material *GetMaterial(const u_int index) {
		return mats[index];
	}
	u_int GetMaterialIndex(const std::string &name);
	u_int GetMaterialIndex(const Material *m) const;
	const std::vector<Material *> &GetMaterials() const {
		return mats;
	}

	u_int GetSize() const { return static_cast<u_int>(mats.size()); }
	std::vector<std::string> GetMaterialNames() const;

	void DeleteMaterial(const std::string &name);
  
private:
	std::vector<Material *> mats;
	boost::unordered_map<std::string, Material *> matsByName;
};

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

class MatteMaterial : public Material {
public:
	MatteMaterial(const Texture *emitted, const Texture *bump,
			const Texture *col) : Material(emitted, bump), Kd(col) { }

	virtual MaterialType GetType() const { return MATTE; }
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

	const Texture *GetKd() const { return Kd; }

private:
	const Texture *Kd;
};

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

class MirrorMaterial : public Material {
public:
	MirrorMaterial(const Texture *emitted, const Texture *bump,
		const Texture *refl) : Material(emitted, bump), Kr(refl) { }

	virtual MaterialType GetType() const { return MIRROR; }
	virtual BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT; };

	virtual bool IsDelta() const { return true; }

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
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Texture *GetKr() const { return Kr; }

private:
	const Texture *Kr;
};

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

class GlassMaterial : public Material {
public:
	GlassMaterial(const Texture *emitted, const Texture *bump,
			const Texture *refl, const Texture *trans,
			const Texture *exteriorIorFact, const Texture *interiorIorFact) :
			Material(emitted, bump),
			Kr(refl), Kt(trans), exteriorIor(exteriorIorFact), interiorIor(interiorIorFact) { }

	virtual MaterialType GetType() const { return GLASS; }
	virtual BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	virtual bool IsDelta() const { return true; }

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
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }
	const Texture *GetExteriorIOR() const { return exteriorIor; }
	const Texture *GetInteriorIOR() const { return interiorIor; }

private:
	const Texture *Kr;
	const Texture *Kt;
	const Texture *exteriorIor;
	const Texture *interiorIor;
};

//------------------------------------------------------------------------------
// Architectural glass material
//------------------------------------------------------------------------------

class ArchGlassMaterial : public Material {
public:
	ArchGlassMaterial(const Texture *emitted, const Texture *bump,
			const Texture *refl, const Texture *trans,
			const Texture *exteriorIorFact, const Texture *interiorIorFact) :
			Material(emitted, bump),
			Kr(refl), Kt(trans), exteriorIor(exteriorIorFact), interiorIor(interiorIorFact) { }

	virtual MaterialType GetType() const { return ARCHGLASS; }
	virtual BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	virtual bool IsDelta() const { return true; }
	virtual bool IsShadowTransparent() const { return true; }
	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint, const luxrays::Vector &localFixedDir,
		const float passThroughEvent) const;

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
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }
	const Texture *GetExteriorIOR() const { return exteriorIor; }
	const Texture *GetInteriorIOR() const { return interiorIor; }

private:
	const Texture *Kr;
	const Texture *Kt;
	const Texture *exteriorIor;
	const Texture *interiorIor;
};

//------------------------------------------------------------------------------
// Mix material
//------------------------------------------------------------------------------

class MixMaterial : public Material {
public:
	MixMaterial(const Texture *bump,
			Material *mA, Material *mB, const Texture *mix) :
			Material(NULL, bump), matA(mA), matB(mB), mixFactor(mix) { }

	virtual MaterialType GetType() const { return MIX; }
	virtual BSDFEvent GetEventTypes() const { return (matA->GetEventTypes() | matB->GetEventTypes()); };

	virtual bool IsLightSource() const {
		return (matA->IsLightSource() || matB->IsLightSource());
	}
	virtual bool HasBumpTex() const { 
		return (matA->HasBumpTex() || matB->HasBumpTex());
	}

	virtual bool IsDelta() const {
		return (matA->IsDelta() && matB->IsDelta());
	}
	virtual bool IsPassThrough() const {
		return (matA->IsPassThrough() || matB->IsPassThrough());
	}
	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent) const;

	virtual const Volume *GetInteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const;
	virtual const Volume *GetExteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const;

	virtual float GetEmittedRadianceY() const;
	virtual luxrays::Spectrum GetEmittedRadiance(const HitPoint &hitPoint,
		const float oneOverPrimitiveArea) const;

    virtual void Bump(HitPoint *hitPoint,
        const luxrays::Vector &dpdu, const luxrays::Vector &dpdv,
        const luxrays::Normal &dndu, const luxrays::Normal &dndv,
        const float weight) const;

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const;
	void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	virtual void UpdateMaterialReferences(Material *oldMat, Material *newMat);
	virtual bool IsReferencing(const Material *mat) const;
	virtual void AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const;
	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Material *GetMaterialA() const { return matA; }
	const Material *GetMaterialB() const { return matB; }
	const Texture *GetMixFactor() const { return mixFactor; }

private:
	Material *matA;
	Material *matB;
	const Texture *mixFactor;
};

//------------------------------------------------------------------------------
// Null material
//------------------------------------------------------------------------------

class NullMaterial : public Material {
public:
	NullMaterial() : Material(NULL, NULL) { }

	virtual MaterialType GetType() const { return NULLMAT; }
	virtual BSDFEvent GetEventTypes() const { return SPECULAR | TRANSMIT; };

	virtual bool IsDelta() const { return true; }
	virtual bool IsPassThrough() const { return true; }
	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent) const { return luxrays::Spectrum(1.f); }

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
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	virtual luxrays::Properties ToProperties() const;
};

//------------------------------------------------------------------------------
// MatteTranslucent material
//------------------------------------------------------------------------------

class MatteTranslucentMaterial : public Material {
public:
	MatteTranslucentMaterial(const Texture *emitted, const Texture *bump,
			const Texture *refl, const Texture *trans) : Material(emitted, bump),
			Kr(refl), Kt(trans) { }

	virtual MaterialType GetType() const { return MATTETRANSLUCENT; }
	virtual BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT | TRANSMIT; };

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

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }

private:
	const Texture *Kr;
	const Texture *Kt;
};

//------------------------------------------------------------------------------
// Glossy2 material
//------------------------------------------------------------------------------

class Glossy2Material : public Material {
public:
	Glossy2Material(const Texture *emitted, const Texture *bump,
			const Texture *kd, const Texture *ks, const Texture *u, const Texture *v,
			const Texture *ka, const Texture *d, const Texture *i, const bool mbounce) :
			Material(emitted, bump), Kd(kd), Ks(ks), nu(u), nv(v),
			Ka(ka), depth(d), index(i), multibounce(mbounce) { }

	virtual MaterialType GetType() const { return GLOSSY2; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

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

	const Texture *GetKd() const { return Kd; }
	const Texture *GetKs() const { return Ks; }
	const Texture *GetNu() const { return nu; }
	const Texture *GetNv() const { return nv; }
	const Texture *GetKa() const { return Ka; }
	const Texture *GetDepth() const { return depth; }
	const Texture *GetIndex() const { return index; }
	const bool IsMultibounce() const { return multibounce; }

private:
	float SchlickBSDF_CoatingWeight(const luxrays::Spectrum &ks, const luxrays::Vector &localFixedDir) const;
	luxrays::Spectrum SchlickBSDF_CoatingF(const bool fromLight, const luxrays::Spectrum &ks, const float roughness, const float anisotropy,
		const luxrays::Vector &localFixedDir,	const luxrays::Vector &localSampledDir) const;
	luxrays::Spectrum SchlickBSDF_CoatingSampleF(const bool fromLight, const luxrays::Spectrum ks,
		const float roughness, const float anisotropy, const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		float u0, float u1, float *pdf) const;
	float SchlickBSDF_CoatingPdf(const float roughness, const float anisotropy,
		const luxrays::Vector &localFixedDir, const luxrays::Vector &localSampledDir) const;

	const Texture *Kd;
	const Texture *Ks;
	const Texture *nu;
	const Texture *nv;
	const Texture *Ka;
	const Texture *depth;
	const Texture *index;
	const bool multibounce;
};

//------------------------------------------------------------------------------
// Metal2 material
//------------------------------------------------------------------------------

class Metal2Material : public Material {
public:
	Metal2Material(const Texture *emitted, const Texture *bump,
			const Texture *nn, const Texture *kk, const Texture *u, const Texture *v) :
			Material(emitted, bump), n(nn), k(kk), nu(u), nv(v) { }

	virtual MaterialType GetType() const { return METAL2; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

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

	const Texture *GetN() const { return n; }
	const Texture *GetK() const { return k; }
	const Texture *GetNu() const { return nu; }
	const Texture *GetNv() const { return nv; }

private:
	const Texture *n;
	const Texture *k;
	const Texture *nu;
	const Texture *nv;
};

//------------------------------------------------------------------------------
// RoughGlass material
//------------------------------------------------------------------------------

class RoughGlassMaterial : public Material {
public:
	RoughGlassMaterial(const Texture *emitted, const Texture *bump,
			const Texture *refl, const Texture *trans,
			const Texture *exteriorIorFact, const Texture *interiorIorFact,
			const Texture *u, const Texture *v) :
			Material(emitted, bump), Kr(refl), Kt(trans),
			exteriorIor(exteriorIorFact), interiorIor(interiorIorFact), nu(u), nv(v) { }

	virtual MaterialType GetType() const { return ROUGHGLASS; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT | TRANSMIT; };

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

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }
	const Texture *GetExteriorIOR() const { return exteriorIor; }
	const Texture *GetInteriorIOR() const { return interiorIor; }
	const Texture *GetNu() const { return nu; }
	const Texture *GetNv() const { return nv; }

private:
	const Texture *Kr;
	const Texture *Kt;
	const Texture *exteriorIor;
	const Texture *interiorIor;
	const Texture *nu;
	const Texture *nv;
};

//------------------------------------------------------------------------------
// Velvet material
//------------------------------------------------------------------------------

class VelvetMaterial : public Material {
public:
	VelvetMaterial(const Texture *emitted, const Texture *bump,
			const Texture *kd, const Texture *p1, const Texture *p2, const Texture *p3, const Texture *thickness) : Material(emitted, bump), Kd(kd), P1(p1), P2(p2), P3(p3), Thickness(thickness) { }

	virtual MaterialType GetType() const { return VELVET; }
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

	const Texture *GetKd() const { return Kd; }
	const Texture *GetP1() const { return P1; }
	const Texture *GetP2() const { return P2; }
	const Texture *GetP3() const { return P3; }
	const Texture *GetThickness() const { return Thickness; }

private:
	const Texture *Kd;
	const Texture *P1;
	const Texture *P2;
	const Texture *P3;
	const Texture *Thickness;
};

//------------------------------------------------------------------------------
// Cloth material
//------------------------------------------------------------------------------

class ClothMaterial : public Material {
public:
	ClothMaterial(const Texture *emitted, const Texture *bump,
            const slg::ocl::ClothPreset preset, const Texture *weft_kd, const Texture *weft_ks,
            const Texture *warp_kd, const Texture *warp_ks, const float repeat_u, const float repeat_v) :
    Material(emitted, bump), Preset(preset), Weft_Kd(weft_kd), Weft_Ks(weft_ks),
            Warp_Kd(warp_kd), Warp_Ks(warp_ks), Repeat_U(repeat_u), Repeat_V(repeat_v) {
        SetPreset();
    }

	virtual MaterialType GetType() const { return CLOTH; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

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

    slg::ocl::ClothPreset GetPreset() const { return Preset; }
	const Texture *GetWeftKd() const { return Weft_Kd; }
	const Texture *GetWeftKs() const { return Weft_Ks; }
	const Texture *GetWarpKd() const { return Warp_Kd; }
	const Texture *GetWarpKs() const { return Warp_Ks; }
	const float GetRepeatU() const { return Repeat_U; }
	const float GetRepeatV() const { return Repeat_V; }
    const float GetSpecularNormalization() const { return specularNormalization; }

private:
	void SetPreset();

	const slg::ocl::Yarn *GetYarn(const float u_i, const float v_i, luxrays::UV *uv, float *umax, float *scale) const;
	void GetYarnUV(const slg::ocl::Yarn *yarn, const luxrays::Point &center, const luxrays::Point &xy, luxrays::UV *uv, float *umaxMod) const;
	
	float RadiusOfCurvature(const slg::ocl::Yarn *yarn, float u, float umaxMod) const;
	float EvalSpecular(const slg::ocl::Yarn *yarn, const luxrays::UV &uv,
        float umax, const luxrays::Vector &wo, const luxrays::Vector &vi) const;
	float EvalIntegrand(const slg::ocl::Yarn *yarn, const luxrays::UV &uv,
        float umaxMod, luxrays::Vector &om_i, luxrays::Vector &om_r) const;
	float EvalFilamentIntegrand(const slg::ocl::Yarn *yarn, const luxrays::Vector &om_i,
        const luxrays::Vector &om_r, float u, float v, float umaxMod) const;
	float EvalStapleIntegrand(const slg::ocl::Yarn *yarn, const luxrays::Vector &om_i,
        const luxrays::Vector &om_r, float u, float v, float umaxMod) const;

	const slg::ocl::ClothPreset Preset;
	const Texture *Weft_Kd;
	const Texture *Weft_Ks;
	const Texture *Warp_Kd;
	const Texture *Warp_Ks;
	const float Repeat_U;
	const float Repeat_V;
	float specularNormalization;
};

//------------------------------------------------------------------------------
// Irawan related functions and classes
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Carpaint material
//------------------------------------------------------------------------------

class CarpaintMaterial : public Material {
public:
	CarpaintMaterial(const Texture *emitted, const Texture *bump,
			const Texture *kd, const Texture *ks1, const Texture *ks2, const Texture *ks3, const Texture *m1, const Texture *m2, const Texture *m3,
			const Texture *r1, const Texture *r2, const Texture *r3, const Texture *ka, const Texture *d) :
			Material(emitted, bump), Kd(kd), Ks1(ks1), Ks2(ks2), Ks3(ks3), M1(m1), M2(m2), M3(m3), R1(r1), R2(r2), R3(r3),
			Ka(ka), depth(d) { }

	virtual MaterialType GetType() const { return CARPAINT; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

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

	struct CarpaintData {
		string name;
		float kd[COLOR_SAMPLES];
		float ks1[COLOR_SAMPLES];
		float ks2[COLOR_SAMPLES];
		float ks3[COLOR_SAMPLES];
		float r1, r2, r3;
		float m1, m2, m3;
	};
	static struct CarpaintData data[8];
	static int NbPresets() { return 8; }

	const Texture *Kd;
	const Texture *Ks1;
	const Texture *Ks2;
	const Texture *Ks3;
	const Texture *M1;
	const Texture *M2;
	const Texture *M3;
	const Texture *R1;
	const Texture *R2;
	const Texture *R3;
	const Texture *Ka;
	const Texture *depth;
};

//------------------------------------------------------------------------------
// Coating absorption
//------------------------------------------------------------------------------

extern luxrays::Spectrum CoatingAbsorption(const float cosi, const float coso,
	const luxrays::Spectrum &alpha, const float depth);

//------------------------------------------------------------------------------
// SchlickDistribution related functions
//------------------------------------------------------------------------------

extern float SchlickDistribution_SchlickZ(const float roughness, const float cosNH);
extern float SchlickDistribution_SchlickA(const luxrays::Vector &H, const float anisotropy);
extern float SchlickDistribution_D(const float roughness, const luxrays::Vector &wh, const float anisotropy);
extern float SchlickDistribution_SchlickG(const float roughness, const float costheta);
extern void SchlickDistribution_SampleH(const float roughness, const float anisotropy,
	const float u0, const float u1, luxrays::Vector *wh, float *d, float *pdf);
extern float SchlickDistribution_Pdf(const float roughness, const luxrays::Vector &wh, const float anisotropy);
extern float SchlickDistribution_G(const float roughness, const luxrays::Vector &localFixedDir,
	const luxrays::Vector &localSampledDir);

//------------------------------------------------------------------------------
// Fresnel related functions
//------------------------------------------------------------------------------

extern luxrays::Spectrum FresnelSchlick_Evaluate(const luxrays::Spectrum &ks, const float cosi);
extern luxrays::Spectrum FresnelGeneral_Evaluate(const luxrays::Spectrum &eta, const luxrays::Spectrum &k, const float cosi);
extern luxrays::Spectrum FresnelCauchy_Evaluate(const float eta, const float cosi);

}

#endif	/* _SLG_MATERIAL_H */
