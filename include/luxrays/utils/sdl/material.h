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

#ifndef _LUXRAYS_SDL_MATERIAL_H
#define	_LUXRAYS_SDL_MATERIAL_H

#include <vector>

#include "luxrays/utils/core/spectrum.h"
#include "luxrays/utils/sdl/bsdfevents.h"
#include "luxrays/utils/core/exttrianglemesh.h"
#include "luxrays/utils/core/mc.h"
#include "luxrays/utils/sdl/texture.h"
#include "texture.h"

namespace luxrays { namespace sdl {

class Scene;

enum MaterialType {
	MATTE, MIRROR, GLASS, METAL, ARCHGLASS, MIX, NULLMAT, MATTETRANSLUCENT
};

class Material {
public:
	Material(const Texture *emitted, const Texture *bump, const Texture *normal) :
		emittedTex(emitted), bumpTex(bump), normalTex(normal) { }
	virtual ~Material() { }

	virtual MaterialType GetType() const = 0;
	virtual BSDFEvent GetEventTypes() const = 0;

	virtual bool IsLightSource() const {
		return (emittedTex != NULL);
	}
	virtual bool HasBumpTex() const { 
		return (bumpTex != NULL);
	}
	virtual bool HasNormalTex() const { 
		return (normalTex != NULL);
	}

	virtual bool IsDelta() const { return false; }
	virtual bool IsPassThrough() const { return false; }
	virtual Spectrum GetPassThroughTransparency(const UV &uv, const float passThroughEvent) const {
		return Spectrum(0.f);
	}

	virtual Spectrum GetEmittedRadiance(const UV &uv) const {
		if (emittedTex)
			return emittedTex->GetColorValue(uv);
		else
			return Spectrum();
	}

	const Texture *GetEmitTexture() const { return emittedTex; }
	const Texture *GetBumpTexture() const { return bumpTex; }
	const Texture *GetNormalTexture() const { return normalTex; }

	virtual Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const = 0;

	virtual Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const = 0;

	virtual void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const = 0;

protected:
	const Texture *emittedTex;
	const Texture *bumpTex;
	const Texture *normalTex;
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
	void UpdateMaterial(const std::string &name, Material *m);

	Material *GetMaterial(const std::string &name);
	Material *GetMaterial(const u_int index) {
		return mats[index];
	}
	u_int GetMaterialIndex(const std::string &name) const;

	u_int GetSize() const { return static_cast<u_int>(mats.size()); }
	vector<std::string> GetMaterialNames() const;
  
private:

	std::vector<Material *> mats;
	std::map<std::string, Material *> matsByName;
	std::map<std::string, u_int> indexByName;
};

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

class MatteMaterial : public Material {
public:
	MatteMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *col) : Material(emitted, bump, normal), Kd(col) { }

	MaterialType GetType() const { return MATTE; }
	BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT; };

	const Texture *GetKd() const { return Kd; }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const;

private:
	const Texture *Kd;
};

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

class MirrorMaterial : public Material {
public:
	MirrorMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
		const Texture *refl) : Material(emitted, bump, normal), Kr(refl) { }

	MaterialType GetType() const { return MIRROR; }
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT; };

	bool IsDelta() const { return true; }

	const Texture *GetKr() const { return Kr; }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

private:
	const Texture *Kr;
};

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

class GlassMaterial : public Material {
public:
	GlassMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *trans,
			const Texture *outsideIorFact, const Texture *iorFact) :
			Material(emitted, bump, normal),
			Kr(refl), Kt(trans), ousideIor(outsideIorFact), ior(iorFact) { }

	MaterialType GetType() const { return GLASS; }
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	bool IsDelta() const { return true; }

	const Texture *GetKrefl() const { return Kr; }
	const Texture *GetKrefrct() const { return Kt; }
	const Texture *GetOutsideIOR() const { return ousideIor; }
	const Texture *GetIOR() const { return ior; }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

private:
	const Texture *Kr;
	const Texture *Kt;
	const Texture *ousideIor;
	const Texture *ior;
};

//------------------------------------------------------------------------------
// Architectural glass material
//------------------------------------------------------------------------------

class ArchGlassMaterial : public Material {
public:
	ArchGlassMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *trans) :
			Material(emitted, bump, normal), Kr(refl), Kt(trans) { }

	MaterialType GetType() const { return ARCHGLASS; }
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	bool IsDelta() const { return true; }
	bool IsShadowTransparent() const { return true; }
	Spectrum GetShadowTransparency(const UV &uv, const float passThroughEvent) const {
		return Kt->GetColorValue(uv);
	}

	const Texture *GetKrefl() const { return Kr; }
	const Texture *GetKrefrct() const { return Kt; }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

private:
	const Texture *Kr;
	const Texture *Kt;
};

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

class MetalMaterial : public Material {
public:
	MetalMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *exp) : Material(emitted, bump, normal),
			Kr(refl), exponent(exp) { }

	MaterialType GetType() const { return METAL; }
	BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

	const Texture *GetKr() const { return Kr; }
	const Texture *GetExp() const { return exponent; }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	static Vector GlossyReflection(const Vector &fixedDir, const float exponent,
			const float u0, const float u1);
private:
	const Texture *Kr;
	const Texture *exponent;
};

//------------------------------------------------------------------------------
// Mix material
//------------------------------------------------------------------------------

class MixMaterial : public Material {
public:
	MixMaterial(const Texture *bump, const Texture *normal,
			const Material *mA, const Material *mB, const Texture *mix) :
			Material(NULL, bump, normal), matA(mA), matB(mB), mixFactor(mix) { }

	MaterialType GetType() const { return MIX; }
	BSDFEvent GetEventTypes() const { return (matA->GetEventTypes() | matB->GetEventTypes()); };

	bool IsLightSource() const {
		return (matA->IsLightSource() || matB->IsLightSource());
	}
	bool IsDelta()  {
		return (matA->IsDelta() && matB->IsDelta());
	}
	bool IsPassThrough() const {
		return (matA->IsPassThrough() || matB->IsPassThrough());
	}
	Spectrum GetPassThroughTransparency(const UV &uv, const float passThroughEvent) const;

	Spectrum GetEmittedRadiance(const UV &uv) const;

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const;

private:
	const Material *matA;
	const Material *matB;
	const Texture *mixFactor;
};

//------------------------------------------------------------------------------
// Null material
//------------------------------------------------------------------------------

class NullMaterial : public Material {
public:
	NullMaterial() : Material(NULL, NULL, NULL) { }

	MaterialType GetType() const { return NULLMAT; }
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	bool IsDelta() const { return true; }
	bool IsPassThrough() const { return true; }
	Spectrum GetPassThroughTransparency(const UV &uv,
		const float passThroughEvent) const { return Spectrum(1.f); }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}
};

//------------------------------------------------------------------------------
// MatteTranslucent material
//------------------------------------------------------------------------------

class MatteTranslucentMaterial : public Material {
public:
	MatteTranslucentMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *trans) : Material(emitted, bump, normal),
			Kr(refl), Kt(trans) { }

	MaterialType GetType() const { return MATTETRANSLUCENT; }
	BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT | TRANSMIT; };

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const;

private:
	const Texture *Kr;
	const Texture *Kt;
};

} }

#endif	/* _LUXRAYS_SDL_MATERIAL_H */
