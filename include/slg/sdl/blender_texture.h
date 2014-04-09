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

#ifndef _SLG_BLENDER_TEXTURE_H
#define	_SLG_BLENDER_TEXTURE_H

#include "slg/sdl/texture.h"
#include "slg/sdl/blender_noiselib.h"

using namespace blender;

namespace slg {

typedef enum {
	TEX_SIN, TEX_SAW, TEX_TRI
} BlenderNoiseBase;

//------------------------------------------------------------------------------
// Blender blend texture
//------------------------------------------------------------------------------
typedef enum {
	TEX_LIN, TEX_QUAD, TEX_EASE, TEX_DIAG, TEX_SPHERE, TEX_HALO, TEX_RAD
} ProgressionType;


class BlenderBlendTexture : public Texture {
public:
	BlenderBlendTexture(const TextureMapping3D *mp, const std::string ptype, const bool direction, float bright, float contrast);
	virtual ~BlenderBlendTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_BLEND; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	ProgressionType GetProgressionType() const { return type; }
	bool GetDirection() const { return direction; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	ProgressionType type;
	bool direction;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender clouds texture
//------------------------------------------------------------------------------

class BlenderCloudsTexture : public Texture {
public:
	BlenderCloudsTexture(const TextureMapping3D *mp, const std::string &pnoisebasis, const float noisesize, const int noisedepth, bool hard, float bright, float contrast);
	virtual ~BlenderCloudsTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_CLOUDS; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	BlenderNoiseBasis GetNoiseBasis() const { return noisebasis; }
	int GetNoiseDepth() const { return noisedepth; }
	float GetNoiseSize() const { return noisesize; }
	bool GetNoiseType() const { return hard; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	BlenderNoiseBasis noisebasis;	
	int noisedepth;	
	float noisesize;	
	bool hard;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender distorted noise texture
//------------------------------------------------------------------------------

class BlenderDistortedNoiseTexture : public Texture {
public:
	BlenderDistortedNoiseTexture(const TextureMapping3D *mp, const std::string &pnoisedistortion,const std::string &pnoisebasis, 
		float distortion, float noisesize, float bright, float contrast);
	virtual ~BlenderDistortedNoiseTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_NOISE; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	BlenderNoiseBasis GetNoiseDistortion() const { return noisedistortion; }
	BlenderNoiseBasis GetNoiseBasis() const { return noisebasis; }
	float GetNoiseSize() const { return noisesize; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	BlenderNoiseBasis noisedistortion;
	BlenderNoiseBasis noisebasis;
	float distortion;
	float noisesize;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender magic texture
//------------------------------------------------------------------------------
class BlenderMagicTexture : public Texture {
public:
	BlenderMagicTexture(const TextureMapping3D *mp, const int noisedepth, const float turbulence, float bright, float contrast);
	virtual ~BlenderMagicTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_BLEND; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	int GetNoiseDepth() const { return noisedepth; }
	float GetTurbulence() const { return turbulence; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	int noisedepth;
	float turbulence;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender marble texture
//------------------------------------------------------------------------------

typedef enum {
	TEX_SOFT, TEX_SHARP, TEX_SHARPER
} BlenderMarbleType;

class BlenderMarbleTexture : public Texture {
public:
	BlenderMarbleTexture(const TextureMapping3D *mp, const std::string &ptype, const std::string &pnoisebasis,
		const std::string &pnoise, float noisesize, float turb, int noisedepth, bool hard,
		float bright, float contrast);
	virtual ~BlenderMarbleTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_WOOD; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	BlenderMarbleType GetMarbleType() const { return type; }
	BlenderNoiseBasis GetNoiseBasis() const { return noisebasis; }
	BlenderNoiseBase GetNoiseBasis2() const { return noisebasis2; }
	float GetNoiseSize() const { return noisesize; }
	float GetTurbulence() const { return turbulence; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }
	bool GetNoiseType() const { return hard; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	BlenderMarbleType type;
	BlenderNoiseBasis noisebasis;	
	BlenderNoiseBase noisebasis2;	
	float noisesize, turbulence;
	int noisedepth;
	bool hard;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender musgrave texture
//------------------------------------------------------------------------------

typedef enum {
	TEX_MULTIFRACTAL, TEX_RIDGED_MULTIFRACTAL, TEX_HYBRID_MULTIFRACTAL, TEX_FBM, TEX_HETERO_TERRAIN
} BlenderMusgraveType;

class BlenderMusgraveTexture : public Texture {
public:
	BlenderMusgraveTexture(const TextureMapping3D *mp, const std::string &ptype, const std::string &pnoisebasis, 
		const float dimension, const float intensity, const float lacunarity, const float offset, const float gain,
		const float octaves, const float noisesize, bool hard, float bright, float contrast);
	virtual ~BlenderMusgraveTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_STUCCI; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	BlenderMusgraveType GetMusgraveType() const { return type; }
	BlenderNoiseBasis GetNoiseBasis() const { return noisebasis; }
	float GetDimension() const { return dimension; }
	float GetIntensity() const { return intensity; }
	float GetLacunarity() const { return lacunarity; }
	float GetOffset() const { return offset; }
	float GetGain() const { return gain; }
	float GetOctaves() const { return octaves; }
	float GetNoiseSize() const { return noisesize; }
	bool GetNoiseType() const { return hard; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	BlenderMusgraveType type;
	BlenderNoiseBasis noisebasis;
	float dimension;
	float intensity;
	float lacunarity;
	float offset;
	float gain;
	float octaves;
	float noisesize;
	bool hard;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender noise texture
//------------------------------------------------------------------------------

class BlenderNoiseTexture : public Texture {
public:
	BlenderNoiseTexture(int noisedepth, float bright, float contrast);
	virtual ~BlenderNoiseTexture() {};

	virtual TextureType GetType() const { return BLENDER_NOISE; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	int GetNoiseDepth() const { return noisedepth; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	int noisedepth;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender stucci texture
//------------------------------------------------------------------------------

typedef enum {
	TEX_PLASTIC, TEX_WALL_IN, TEX_WALL_OUT
} BlenderStucciType;

class BlenderStucciTexture : public Texture {
public:
	BlenderStucciTexture(const TextureMapping3D *mp, const std::string &ptype, const std::string &pnoisebasis, 
		const float noisesize, float turb, bool hard, float bright, float contrast);
	virtual ~BlenderStucciTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_STUCCI; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	BlenderStucciType GetStucciType() const { return type; }
	BlenderNoiseBasis GetNoiseBasis() const { return noisebasis; }
	float GetNoiseSize() const { return noisesize; }
	float GetTurbulence() const { return turbulence; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }
	bool GetNoiseType() const { return hard; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	BlenderStucciType type;
	BlenderNoiseBasis noisebasis;	
	float noisesize, turbulence;
	bool hard;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender wood texture
//------------------------------------------------------------------------------

typedef enum {
	BANDS, RINGS, BANDNOISE, RINGNOISE
} BlenderWoodType;

class BlenderWoodTexture : public Texture {
public:
	BlenderWoodTexture(const TextureMapping3D *mp, const std::string &ptype, const std::string &pnoise, const float noisesize, float turb, bool hard, float bright, float contrast);
	virtual ~BlenderWoodTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_WOOD; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	BlenderWoodType GetWoodType() const { return type; }
	BlenderNoiseBase GetNoiseBasis2() const { return noisebasis2; }
	float GetNoiseSize() const { return noisesize; }
	float GetTurbulence() const { return turbulence; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }
	bool GetNoiseType() const { return hard; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	BlenderWoodType type;
	BlenderWoodNoiseBase noisebasis2;	
	float noisesize, turbulence;
	bool hard;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender voronoi texture
//------------------------------------------------------------------------------

class BlenderVoronoiTexture : public Texture {
public:
	BlenderVoronoiTexture(const TextureMapping3D *mp, const float intensity, const float exponent, const float fw1, const float fw2, const float fw3, const float fw4, 
		const DistanceMetric distmetric, const float noisesize, float bright, float contrast);
	virtual ~BlenderVoronoiTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_VORONOI; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }
	string GetDistMetric() const {
		std::string result = "";
		switch(distancemetric) {
			case DISTANCE_SQUARED:
				result = "distance_squared";
			case MANHATTAN:
				result = "manhattan";
			case CHEBYCHEV:
				result = "chebychev";
			case MINKOWSKI_HALF:
				result = "minkowski_half";
			case MINKOWSKI_FOUR:
				result = "minkowski_four";
			case MINKOWSKI:
				result = "minkowski";
			case ACTUAL_DISTANCE: 
			default:
				result = "actual_distance";

			return result;
		}
	}
	float GetFeatureWeight1() const { return feature_weight1; }
	float GetFeatureWeight2() const { return feature_weight2; }
	float GetFeatureWeight3() const { return feature_weight3; }
	float GetFeatureWeight4() const { return feature_weight4; }
	float GetNoiseSize() const { return noisesize; }
	float GetIntensity() const { return intensity; }
	float GetExponent() const { return exponent; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;

	float intensity;
	float feature_weight1;
	float feature_weight2;
	float feature_weight3;
	float feature_weight4;
	DistanceMetric distancemetric;
	float exponent;
	float noisesize;
	float bright, contrast;
};

}

#endif	/* _SLG_BLENDER_TEXTURE_H */
