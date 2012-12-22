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

#ifndef _LUXRAYS_SDL_TEXTURE_H
#define	_LUXRAYS_SDL_TEXTURE_H

#include "luxrays/core/geometry/uv.h"
#include "luxrays/utils/core/spectrum.h"

namespace luxrays { namespace sdl {

//------------------------------------------------------------------------------
// Texture
//------------------------------------------------------------------------------

enum TextureType {
	CONST_FLOAT, CONST_FLOAT3, CONST_FLOAT4, IMAGEMAP
};
	
class Texture {
public:
	Texture() { }
	virtual ~Texture() { }

	virtual float GetGreyValue(const UV &uv) const = 0;
	virtual Spectrum GetColorValue(const UV &uv) const = 0;
	virtual float GetAlphaValue(const UV &uv) const = 0;
};

//------------------------------------------------------------------------------
// TextureDefinitions
//------------------------------------------------------------------------------

class TextureDefinitions {
public:
	TextureDefinitions() { }
	~TextureDefinitions() {
		for (std::map<std::string, Texture *>::const_iterator it = texs.begin(); it != texs.end(); ++it)
			delete it->second;
	}

	void DefineTexture(const std::string &name, Texture *t) {
		texs.insert(std::make_pair(name, t));
	}
	const Texture *GetTexture(const std::string &name) {
		// Check if the texture map has been already loaded
		std::map<std::string, Texture *>::const_iterator it = texs.find(name);

		if (it == texs.end())
			throw std::runtime_error("Reference to an undefined texture: " + name);
		else
			return it->second;
	}

	u_int GetSize()const { return static_cast<u_int>(texs.size()); }
  
private:

	std::map<std::string, Texture *> texs;
};

//------------------------------------------------------------------------------
// Constant textures
//------------------------------------------------------------------------------

class ConstFloatTexture : public Texture {
public:
	ConstFloatTexture(const float v) : value(v) { }
	virtual ~ConstFloatTexture() { }

	virtual float GetGreyValue(const UV &uv) const { return value; }
	virtual Spectrum GetColorValue(const UV &uv) const { return Spectrum(value); }
	virtual float GetAlphaValue(const UV &uv) const { return value; }

private:
	float value;
};

class ConstFloat3Texture : public Texture {
public:
	ConstFloat3Texture(const Spectrum &c) : color(c) { }
	virtual ~ConstFloat3Texture() { }

	virtual float GetGreyValue(const UV &uv) const { return color.Y(); }
	virtual Spectrum GetColorValue(const UV &uv) const { return color; }
	virtual float GetAlphaValue(const UV &uv) const { return 1.f; }

private:
	Spectrum color;
};

class ConstFloat4Texture : public Texture {
public:
	ConstFloat4Texture(const Spectrum &c, const float a) : color(c), alpha(a) { }
	virtual ~ConstFloat4Texture() { }

	virtual float GetGreyValue(const UV &uv) const { return color.Y(); }
	virtual Spectrum GetColorValue(const UV &uv) const { return color; }
	virtual float GetAlphaValue(const UV &uv) const { return alpha; }

private:
	Spectrum color;
	float alpha;
};

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

class ImageMap {
public:
	ImageMap(const std::string &fileName, const float gamma);
	ImageMap(float *cols, const float gamma, const u_int channels, const u_int width, const u_int height);
	~ImageMap();

	float GetGamma() const { return gamma; }
	u_int GetChannelCount() const { return channelCount; }
	u_int GetWidth() const { return width; }
	u_int GetHeight() const { return height; }
	const float *GetPixels() const { return pixels; }

	friend class ImageMapInstance;

protected:
	float GetGrey(const UV &uv) const {
		const float s = uv.u * width - 0.5f;
		const float t = uv.v * height - 0.5f;

		const int s0 = Floor2Int(s);
		const int t0 = Floor2Int(t);

		const float ds = s - s0;
		const float dt = t - t0;

		const float ids = 1.f - ds;
		const float idt = 1.f - dt;

		return ids * idt * GetGreyTexel(s0, t0) +
				ids * dt * GetGreyTexel(s0, t0 + 1) +
				ds * idt * GetGreyTexel(s0 + 1, t0) +
				ds * dt * GetGreyTexel(s0 + 1, t0 + 1);
	}

	Spectrum GetColor(const UV &uv) const {
		const float s = uv.u * width - 0.5f;
		const float t = uv.v * height - 0.5f;

		const int s0 = Floor2Int(s);
		const int t0 = Floor2Int(t);

		const float ds = s - s0;
		const float dt = t - t0;

		const float ids = 1.f - ds;
		const float idt = 1.f - dt;

		return ids * idt * GetColorTexel(s0, t0) +
				ids * dt * GetColorTexel(s0, t0 + 1) +
				ds * idt * GetColorTexel(s0 + 1, t0) +
				ds * dt * GetColorTexel(s0 + 1, t0 + 1);
	};

	float GetAlpha(const UV &uv) const {
		const float s = uv.u * width - 0.5f;
		const float t = uv.v * height - 0.5f;

		const int s0 = Floor2Int(s);
		const int t0 = Floor2Int(t);

		const float ds = s - s0;
		const float dt = t - t0;

		const float ids = 1.f - ds;
		const float idt = 1.f - dt;

		return ids * idt * GetAlphaTexel(s0, t0) +
				ids * dt * GetAlphaTexel(s0, t0 + 1) +
				ds * idt * GetAlphaTexel(s0 + 1, t0) +
				ds * dt * GetAlphaTexel(s0 + 1, t0 + 1);
	}

private:
	float GetGreyTexel(const int s, const int t) const {
		const u_int u = Mod<int>(s, width);
		const u_int v = Mod<int>(t, height);

		const unsigned index = v * width + u;
		assert (index >= 0);
		assert (index < width * height);

		if (channelCount == 1)
			return Spectrum(pixels[index]).Y();
		else {
			// channelCount = (3 or 4)
			const float *pixel = &pixels[index * channelCount];
			return Spectrum(pixel[0], pixel[1], pixel[2]).Y();
		}
	}

	Spectrum GetColorTexel(const int s, const int t) const {
		const u_int u = Mod<int>(s, width);
		const u_int v = Mod<int>(t, height);

		const unsigned index = v * width + u;
		assert (index >= 0);
		assert (index < width * height);

		if (channelCount == 1) 
			return Spectrum(pixels[index]);
		else {
			// channelCount = (3 or 4)
			const float *pixel = &pixels[index * channelCount];
			return Spectrum(pixel[0], pixel[1], pixel[2]);
		}
	}

	float GetAlphaTexel(const int s, const int t) const {
		if (channelCount == 4) {
			const u_int u = Mod<int>(s, width);
			const u_int v = Mod<int>(t, height);

			const unsigned index = v * width + u;
			assert (index >= 0);
			assert (index < width * height);

			return pixels[index * channelCount + 3];
		} else
			return 1.f;
	}

	float gamma;
	u_int channelCount, width, height;
	float *pixels;
};

class ImageMapInstance {
public:
	ImageMapInstance(const ImageMap *im, const float gn,
			const float uscale, const float vscale,
			const float udelta, const float vdelta) : imgMap(im), gain(gn),
		uScale(uscale), vScale(vscale), uDelta(udelta), vDelta(vdelta) {
		DuDv.u = 1.f / (uScale * imgMap->GetWidth());
		DuDv.v = 1.f / (vScale * imgMap->GetHeight());
	}
	~ImageMapInstance() { }

	const ImageMap *GetImgMap() const { return imgMap; }
	float GetUScale() const { return uScale; }
	float GetVScale() const { return vScale; }
	float GetUDelta() const { return uDelta; }
	float GetVDelta() const { return vDelta; }

	float GetGrey(const UV &uv) const {
		const UV mapUV(uv.u * uScale + uDelta, uv.v * vScale + vDelta);

		return gain * imgMap->GetGrey(mapUV);
	}

	Spectrum GetColor(const UV &uv) const {
		const UV mapUV(uv.u * uScale + uDelta, uv.v * vScale + vDelta);

		return gain * imgMap->GetColor(mapUV);
	}

	float GetAlpha(const UV &uv) const {
		const UV mapUV(uv.u * uScale + uDelta, uv.v * vScale + vDelta);

		return imgMap->GetAlpha(mapUV);
	}

	const UV &GetDuDv() const { return DuDv; }

protected:
	const ImageMap *imgMap;
	float gain, uScale, vScale, uDelta, vDelta;
	UV DuDv;
};

class ImageMapCache {
public:
	ImageMapCache();
	~ImageMapCache();

	void DefineImgMap(const std::string &name, ImageMap *im);

	ImageMapInstance *GetImageMapInstance(const std::string &fileName, const float gamma, const float gain = 1.f,
		const float uScale = 1.f, const float vScale = 1.f, const float uDelta = 0.f, const float vDelta = 0.f);

	void GetImageMaps(std::vector<ImageMap *> &tms);
	u_int GetSize()const { return static_cast<u_int>(maps.size()); }
  
private:
	ImageMap *GetImageMap(const std::string &fileName, const float gamma);

	std::map<std::string, ImageMap *> maps;
	std::vector<ImageMapInstance *> imgMapInstances;
};

class ImageMapTexture : public Texture {
public:
	ImageMapTexture(const ImageMapInstance * imi) : imgMapInstance(imi) { }
	virtual ~ImageMapTexture() { }

	virtual float GetGreyValue(const UV &uv) const { return imgMapInstance->GetGrey(uv); }
	virtual Spectrum GetColorValue(const UV &uv) const { return imgMapInstance->GetColor(uv); }
	virtual float GetAlphaValue(const UV &uv) const { return imgMapInstance->GetAlpha(uv); }

private:
	const ImageMapInstance *imgMapInstance;
};

} }

#endif	/* _LUXRAYS_SDL_TEXTURE_H */
