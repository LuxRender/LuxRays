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

#ifndef _SLG_SCENE_H
#define	_SLG_SCENE_H

#include <string>
#include <iostream>
#include <fstream>

#include "luxrays/utils/properties.h"
#include "luxrays/core/extmeshcache.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/accelerator.h"
#include "slg/camera/camera.h"
#include "slg/editaction.h"
#include "slg/sdl/sdl.h"
#include "slg/sdl/light.h"
#include "slg/sdl/texture.h"
#include "slg/sdl/material.h"
#include "slg/sdl/sceneobject.h"
#include "slg/sdl/bsdf.h"
#include "slg/sdl/mapping.h"
#include "slg/core/mc.h"

namespace slg {

class Scene {
public:
	// Constructor used to create a scene by calling methods
	Scene(const float imageScale = 1.f);
	// Constructor used to load a scene from file
	Scene(const std::string &fileName, const float imageScale = 1.f);
	~Scene();

	const luxrays::Properties &GetProperties() const { return sceneProperties; }

	bool Intersect(luxrays::IntersectionDevice *device, const bool fromLight,
		const float u0, luxrays::Ray *ray, luxrays::RayHit *rayHit,
		BSDF *bsdf, luxrays::Spectrum *connectionThroughput) const;

	void Preprocess(luxrays::Context *ctx, const u_int filmWidth, const u_int filmHeight);

	luxrays::Properties ToProperties(const std::string &directoryName);

	//--------------------------------------------------------------------------
	// Methods to build and edit scene
	//--------------------------------------------------------------------------

	void DefineImageMap(const std::string &name, ImageMap *im);
	void DefineImageMap(const std::string &name, float *cols, const float gamma,
		const u_int channels, const u_int width, const u_int height);
	bool IsImageMapDefined(const std::string &imgMapName) const;

	void DefineMesh(const std::string &meshName, luxrays::ExtTriangleMesh *mesh);
	void DefineMesh(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		luxrays::Point *p, luxrays::Triangle *vi, luxrays::Normal *n, luxrays::UV *uv,
		luxrays::Spectrum *cols, float *alphas);
	bool IsMeshDefined(const std::string &meshName) const;

	bool IsTextureDefined(const std::string &texName) const;
	bool IsMaterialDefined(const std::string &matName) const;

	void Parse(const luxrays::Properties &props);
	void DeleteObject(const std::string &objName);

	void UpdateObjectTransformation(const std::string &objName, const luxrays::Transform &trans);

	void RemoveUnusedImageMaps();
	void RemoveUnusedTextures();
	void RemoveUnusedMaterials();
	void RemoveUnusedMeshes();

	//--------------------------------------------------------------------------

	Camera *camera;

	luxrays::ExtMeshCache extMeshCache; // Mesh objects cache
	ImageMapCache imgMapCache; // Image maps cache

	TextureDefinitions texDefs; // Texture definitions
	MaterialDefinitions matDefs; // Material definitions
	SceneObjectDefinitions objDefs; // SceneObject definitions
	LightSourceDefinitions lightDefs; // LightSource definitions

	luxrays::DataSet *dataSet;
	luxrays::AcceleratorType accelType;
	bool enableInstanceSupport;

	EditActionList editActions;

protected:
	void ParseCamera(const luxrays::Properties &props);
	void ParseTextures(const luxrays::Properties &props);
	void ParseMaterials(const luxrays::Properties &props);
	void ParseObjects(const luxrays::Properties &props);
	void ParseLights(const luxrays::Properties &props);

	TextureMapping2D *CreateTextureMapping2D(const std::string &prefixName, const luxrays::Properties &props);
	TextureMapping3D *CreateTextureMapping3D(const std::string &prefixName, const luxrays::Properties &props);
	Texture *CreateTexture(const std::string &texName, const luxrays::Properties &props);
	Texture *GetTexture(const luxrays::Property &name);
	Material *CreateMaterial(const u_int defaultMatID, const std::string &matName, const luxrays::Properties &props);
	SceneObject *CreateObject(const std::string &objName, const luxrays::Properties &props);
	LightSource *CreateLightSource(const std::string &lightName, const luxrays::Properties &props);

	luxrays::Properties sceneProperties;
};

}

#endif	/* _SLG_SCENE_H */
