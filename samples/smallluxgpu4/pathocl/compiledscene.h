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

#ifndef _SLG_COMPILEDSESSION_H
#define	_SLGCOMPILEDSESSION_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg.h"
#include "ocldatatypes.h"
#include "editaction.h"

#include "luxrays/utils/film/film.h"
#include "luxrays/utils/sdl/scene.h"

namespace slg {

class CompiledScene {
public:
	CompiledScene(Scene *scn, Film *flm, const size_t maxMemPageS);
	~CompiledScene();

	void Recompile(const EditActionList &editActions);
	bool IsMaterialCompiled(const MaterialType type) const;

	Scene *scene;
	Film *film;
	u_int maxMemPageSize;

	// Compiled Camera
	luxrays::ocl::Camera camera;

	// Compiled Scene Geometry
	vector<Point> verts;
	vector<Normal> normals;
	vector<UV> uvs;
	vector<Triangle> tris;
	vector<luxrays::ocl::Mesh> meshDescs;
	const TriangleMeshID *meshIDs;
	// One element for each mesh, it used to translate the RayBuffer currentTriangleIndex
	// to mesh TriangleID
	u_int *meshFirstTriangleOffset;

	// Compiled AreaLights
	vector<slg::ocl::TriangleLight> areaLights;

	// Compiled InfiniteLights
	slg::ocl::InfiniteLight *infiniteLight;
	const Spectrum *infiniteLightMap;

	// Compiled SunLight
	slg::ocl::SunLight *sunLight;
	// Compiled SkyLight
	slg::ocl::SkyLight *skyLight;

//	// Compiled Materials
//	bool enable_MAT_MATTE, enable_MAT_AREALIGHT, enable_MAT_MIRROR, enable_MAT_GLASS,
//		enable_MAT_MATTEMIRROR, enable_MAT_METAL, enable_MAT_MATTEMETAL, enable_MAT_ALLOY,
//		enable_MAT_ARCHGLASS;
//	vector<PathOCL::Material> mats;
//	vector<u_int> meshMats;

//	// Compiled TextureMaps
//	vector<PathOCL::TexMap> gpuTexMaps;
//	u_int totRGBTexMem, totAlphaTexMem;
//	vector<vector<Spectrum> > rgbTexMemBlocks;
//	vector<vector<float> > alphaTexMemBlocks;

//	vector<u_int> meshTexMaps;
//	vector<PathOCL::TexMapInfo> meshTexMapsInfo;
//	// Compiled BumpMaps
//	vector<u_int> meshBumpMaps;
//	vector<PathOCL::BumpMapInfo> meshBumpMapsInfo;
//	// Compiled NormalMaps
//	vector<u_int> meshNormalMaps;
//	vector<PathOCL::NormalMapInfo> meshNormalMapsInfo;

private:
	void CompileCamera();
	void CompileGeometry();
	void CompileMaterials();
	void CompileAreaLights();
	void CompileInfiniteLight();
	void CompileSunLight();
	void CompileSkyLight();
	void CompileTextureMaps();
};

}

#endif

#endif	/* _SLG_COMPILEDSESSION_H */