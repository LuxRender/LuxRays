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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include"compiledscene.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

namespace slg {

CompiledScene::CompiledScene(Scene *scn, Film *flm, const size_t maxMemPageS) {
	scene = scn;
	film = flm;
	maxMemPageSize = (u_int)Min<size_t>(maxMemPageS, 0xffffffffu);

//	infiniteLight = NULL;
//	infiniteLightMap = NULL;
//	sunLight = NULL;
//	skyLight = NULL;
//	rgbTexMemBlocks.resize(0);
//	alphaTexMemBlocks.resize(0);
//
//	meshTexMaps.resize(0);
//	meshTexMapsInfo.resize(0);
//	meshBumpMaps.resize(0);
//	meshBumpMapsInfo.resize(0);
//	meshNormalMaps.resize(0);
//	meshNormalMapsInfo.resize(0);
	
	meshFirstTriangleOffset = NULL;

	EditActionList editActions;
	editActions.AddAllAction();
	Recompile(editActions);
}

CompiledScene::~CompiledScene() {
	delete[] meshFirstTriangleOffset;
//	delete infiniteLight;
//	// infiniteLightMap memory is handled from another class
//	delete sunLight;
//	delete skyLight;
}

void CompiledScene::CompileCamera() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile Camera");

	//--------------------------------------------------------------------------
	// Camera definition
	//--------------------------------------------------------------------------

	camera.yon = scene->camera->clipYon;
	camera.hither = scene->camera->clipHither;
	camera.lensRadius = scene->camera->lensRadius;
	camera.focalDistance = scene->camera->focalDistance;
	memcpy(&camera.rasterToCameraMatrix[0][0], scene->camera->GetRasterToCameraMatrix().m, 4 * 4 * sizeof(float));
	memcpy(&camera.cameraToWorldMatrix[0][0], scene->camera->GetCameraToWorldMatrix().m, 4 * 4 * sizeof(float));
}

static bool MeshPtrCompare(Mesh *p0, Mesh *p1) {
	return p0 < p1;
}

void CompiledScene::CompileGeometry() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile Geometry");

	const u_int verticesCount = scene->dataSet->GetTotalVertexCount();
	const u_int trianglesCount = scene->dataSet->GetTotalTriangleCount();
	const u_int objCount = scene->meshDefs.GetSize();
	std::vector<ExtMesh *> &objs = scene->meshDefs.GetAllMesh();

	const double tStart = WallClockTime();

	// Clear vectors
	verts.resize(0);
	normals.resize(0);
	uvs.resize(0);
	tris.resize(0);
	meshDescs.resize(0);

	meshIDs = scene->dataSet->GetMeshIDTable();

	// Check the used accelerator type
	if (scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH) {
		// MQBVH geometry must be defined in a specific way.

		//----------------------------------------------------------------------
		// Translate mesh IDs
		//----------------------------------------------------------------------

		delete[] meshFirstTriangleOffset;

		// This is a bit a trick, it does some assumption about how the merge
		// of Mesh works
		meshFirstTriangleOffset = new TriangleID[objCount];
		size_t currentIndex = 0;
		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];
			meshFirstTriangleOffset[i] = currentIndex;
			currentIndex += mesh->GetTotalTriangleCount();
		}

		//----------------------------------------------------------------------
		// Translate geometry
		//----------------------------------------------------------------------

		std::map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)> definedMeshs(MeshPtrCompare);

		luxrays::ocl::Mesh newMeshDesc;
		newMeshDesc.vertsOffset = 0;
		newMeshDesc.trisOffset = 0;
		memcpy(newMeshDesc.trans, Matrix4x4().m, sizeof(float[4][4]));
		memcpy(newMeshDesc.invTrans, Matrix4x4().m, sizeof(float[4][4]));

		luxrays::ocl::Mesh currentMeshDesc;

		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];

			bool isExistingInstance;
			if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
				ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;

				// Check if is one of the already defined meshes
				std::map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)>::iterator it = definedMeshs.find(imesh->GetExtTriangleMesh());
				if (it == definedMeshs.end()) {
					// It is a new one
					currentMeshDesc = newMeshDesc;

					newMeshDesc.vertsOffset += imesh->GetTotalVertexCount();
					newMeshDesc.trisOffset += imesh->GetTotalTriangleCount();

					isExistingInstance = false;

					const u_int index = meshDescs.size();
					definedMeshs[imesh->GetExtTriangleMesh()] = index;
				} else {
					currentMeshDesc = meshDescs[it->second];

					isExistingInstance = true;
				}

				memcpy(currentMeshDesc.trans, imesh->GetTransformation().GetMatrix().m, sizeof(float[4][4]));
				mesh = imesh->GetExtTriangleMesh();
			} else {
				currentMeshDesc = newMeshDesc;

				newMeshDesc.vertsOffset += mesh->GetTotalVertexCount();
				newMeshDesc.trisOffset += mesh->GetTotalTriangleCount();

				isExistingInstance = false;
			}

			meshDescs.push_back(currentMeshDesc);

			if (!isExistingInstance) {
				assert (mesh->GetType() == TYPE_EXT_TRIANGLE);

				//--------------------------------------------------------------
				// Translate mesh normals
				//--------------------------------------------------------------

				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					normals.push_back(mesh->GetShadeNormal(j));

				//----------------------------------------------------------------------
				// Translate vertex uvs
				//----------------------------------------------------------------------

				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
					if (mesh->HasUVs())
						uvs.push_back(mesh->GetUV(j));
					else
						uvs.push_back(UV(0.f, 0.f));
				}

				//--------------------------------------------------------------
				// Translate mesh vertices
				//--------------------------------------------------------------

				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					verts.push_back(mesh->GetVertex(j));

				//--------------------------------------------------------------
				// Translate mesh indices
				//--------------------------------------------------------------

				Triangle *mtris = mesh->GetTriangles();
				for (u_int j = 0; j < mesh->GetTotalTriangleCount(); ++j)
					tris.push_back(mtris[j]);
			}
		}
	} else {
		meshFirstTriangleOffset = NULL;

		//----------------------------------------------------------------------
		// Translate mesh normals
		//----------------------------------------------------------------------

		normals.reserve(verticesCount);
		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];

			for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
				normals.push_back(mesh->GetShadeNormal(j));
		}

		//----------------------------------------------------------------------
		// Translate vertex uvs
		//----------------------------------------------------------------------

		uvs.reserve(verticesCount);
		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];

			for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
				if (mesh->HasUVs())
					uvs.push_back(mesh->GetUV(j));
				else
					uvs.push_back(UV(0.f, 0.f));
			}
		}

		//----------------------------------------------------------------------
		// Translate mesh vertices
		//----------------------------------------------------------------------

		u_int *meshOffsets = new u_int[objCount];
		verts.reserve(verticesCount);
		u_int vIndex = 0;
		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];

			meshOffsets[i] = vIndex;
			for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
				verts.push_back(mesh->GetVertex(j));

			vIndex += mesh->GetTotalVertexCount();
		}

		//----------------------------------------------------------------------
		// Translate mesh indices
		//----------------------------------------------------------------------

		tris.reserve(trianglesCount);
		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];

			Triangle *mtris = mesh->GetTriangles();
			const u_int moffset = meshOffsets[i];
			for (u_int j = 0; j < mesh->GetTotalTriangleCount(); ++j) {
				tris.push_back(Triangle(
						mtris[j].v[0] + moffset,
						mtris[j].v[1] + moffset,
						mtris[j].v[2] + moffset));
			}
		}
		delete[] meshOffsets;
	}

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Scene geometry compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::CompileMaterials() {
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile Materials");
//
//	//--------------------------------------------------------------------------
//	// Translate material definitions
//	//--------------------------------------------------------------------------
//
//	const double tStart = WallClockTime();
//
//	enable_MAT_MATTE = false;
//	enable_MAT_AREALIGHT = false;
//	enable_MAT_MIRROR = false;
//	enable_MAT_GLASS = false;
//	enable_MAT_MATTEMIRROR = false;
//	enable_MAT_METAL = false;
//	enable_MAT_MATTEMETAL = false;
//	enable_MAT_ALLOY = false;
//	enable_MAT_ARCHGLASS = false;
//
//	const u_int materialsCount = scene->materials.size();
//	mats.resize(materialsCount);
//
//	for (u_int i = 0; i < materialsCount; ++i) {
//		Material *m = scene->materials[i];
//		PathOCL::Material *gpum = &mats[i];
//
//		switch (m->GetType()) {
//			case MATTE: {
//				enable_MAT_MATTE = true;
//				MatteMaterial *mm = (MatteMaterial *)m;
//
//				gpum->type = MAT_MATTE;
//				gpum->param.matte.r = mm->GetKd().r;
//				gpum->param.matte.g = mm->GetKd().g;
//				gpum->param.matte.b = mm->GetKd().b;
//				break;
//			}
//			case AREALIGHT: {
//				enable_MAT_AREALIGHT = true;
//				AreaLightMaterial *alm = (AreaLightMaterial *)m;
//
//				gpum->type = MAT_AREALIGHT;
//				gpum->param.areaLight.gain_r = alm->GetGain().r;
//				gpum->param.areaLight.gain_g = alm->GetGain().g;
//				gpum->param.areaLight.gain_b = alm->GetGain().b;
//				break;
//			}
//			case MIRROR: {
//				enable_MAT_MIRROR = true;
//				MirrorMaterial *mm = (MirrorMaterial *)m;
//
//				gpum->type = MAT_MIRROR;
//				gpum->param.mirror.r = mm->GetKr().r;
//				gpum->param.mirror.g = mm->GetKr().g;
//				gpum->param.mirror.b = mm->GetKr().b;
//				gpum->param.mirror.specularBounce = mm->HasSpecularBounceEnabled();
//				break;
//			}
//			case GLASS: {
//				enable_MAT_GLASS = true;
//				GlassMaterial *gm = (GlassMaterial *)m;
//
//				gpum->type = MAT_GLASS;
//				gpum->param.glass.refl_r = gm->GetKrefl().r;
//				gpum->param.glass.refl_g = gm->GetKrefl().g;
//				gpum->param.glass.refl_b = gm->GetKrefl().b;
//
//				gpum->param.glass.refrct_r = gm->GetKrefrct().r;
//				gpum->param.glass.refrct_g = gm->GetKrefrct().g;
//				gpum->param.glass.refrct_b = gm->GetKrefrct().b;
//
//				gpum->param.glass.ousideIor = gm->GetOutsideIOR();
//				gpum->param.glass.ior = gm->GetIOR();
//				gpum->param.glass.R0 = gm->GetR0();
//				gpum->param.glass.reflectionSpecularBounce = gm->HasReflSpecularBounceEnabled();
//				gpum->param.glass.transmitionSpecularBounce = gm->HasRefrctSpecularBounceEnabled();
//				break;
//			}
//			case MATTEMIRROR: {
//				enable_MAT_MATTEMIRROR = true;
//				MatteMirrorMaterial *mmm = (MatteMirrorMaterial *)m;
//
//				gpum->type = MAT_MATTEMIRROR;
//				gpum->param.matteMirror.matte.r = mmm->GetMatte().GetKd().r;
//				gpum->param.matteMirror.matte.g = mmm->GetMatte().GetKd().g;
//				gpum->param.matteMirror.matte.b = mmm->GetMatte().GetKd().b;
//
//				gpum->param.matteMirror.mirror.r = mmm->GetMirror().GetKr().r;
//				gpum->param.matteMirror.mirror.g = mmm->GetMirror().GetKr().g;
//				gpum->param.matteMirror.mirror.b = mmm->GetMirror().GetKr().b;
//				gpum->param.matteMirror.mirror.specularBounce = mmm->GetMirror().HasSpecularBounceEnabled();
//
//				gpum->param.matteMirror.matteFilter = mmm->GetMatteFilter();
//				gpum->param.matteMirror.totFilter = mmm->GetTotFilter();
//				gpum->param.matteMirror.mattePdf = mmm->GetMattePdf();
//				gpum->param.matteMirror.mirrorPdf = mmm->GetMirrorPdf();
//				break;
//			}
//			case METAL: {
//				enable_MAT_METAL = true;
//				MetalMaterial *mm = (MetalMaterial *)m;
//
//				gpum->type = MAT_METAL;
//				gpum->param.metal.r = mm->GetKr().r;
//				gpum->param.metal.g = mm->GetKr().g;
//				gpum->param.metal.b = mm->GetKr().b;
//				gpum->param.metal.exponent = mm->GetExp();
//				gpum->param.metal.specularBounce = mm->HasSpecularBounceEnabled();
//				break;
//			}
//			case MATTEMETAL: {
//				enable_MAT_MATTEMETAL = true;
//				MatteMetalMaterial *mmm = (MatteMetalMaterial *)m;
//
//				gpum->type = MAT_MATTEMETAL;
//				gpum->param.matteMetal.matte.r = mmm->GetMatte().GetKd().r;
//				gpum->param.matteMetal.matte.g = mmm->GetMatte().GetKd().g;
//				gpum->param.matteMetal.matte.b = mmm->GetMatte().GetKd().b;
//
//				gpum->param.matteMetal.metal.r = mmm->GetMetal().GetKr().r;
//				gpum->param.matteMetal.metal.g = mmm->GetMetal().GetKr().g;
//				gpum->param.matteMetal.metal.b = mmm->GetMetal().GetKr().b;
//				gpum->param.matteMetal.metal.exponent = mmm->GetMetal().GetExp();
//				gpum->param.matteMetal.metal.specularBounce = mmm->GetMetal().HasSpecularBounceEnabled();
//
//				gpum->param.matteMetal.matteFilter = mmm->GetMatteFilter();
//				gpum->param.matteMetal.totFilter = mmm->GetTotFilter();
//				gpum->param.matteMetal.mattePdf = mmm->GetMattePdf();
//				gpum->param.matteMetal.metalPdf = mmm->GetMetalPdf();
//				break;
//			}
//			case ALLOY: {
//				enable_MAT_ALLOY = true;
//				AlloyMaterial *am = (AlloyMaterial *)m;
//
//				gpum->type = MAT_ALLOY;
//				gpum->param.alloy.refl_r= am->GetKrefl().r;
//				gpum->param.alloy.refl_g = am->GetKrefl().g;
//				gpum->param.alloy.refl_b = am->GetKrefl().b;
//
//				gpum->param.alloy.diff_r = am->GetKd().r;
//				gpum->param.alloy.diff_g = am->GetKd().g;
//				gpum->param.alloy.diff_b = am->GetKd().b;
//
//				gpum->param.alloy.exponent = am->GetExp();
//				gpum->param.alloy.R0 = am->GetR0();
//				gpum->param.alloy.specularBounce = am->HasSpecularBounceEnabled();
//				break;
//			}
//			case ARCHGLASS: {
//				enable_MAT_ARCHGLASS = true;
//				ArchGlassMaterial *agm = (ArchGlassMaterial *)m;
//
//				gpum->type = MAT_ARCHGLASS;
//				gpum->param.archGlass.refl_r = agm->GetKrefl().r;
//				gpum->param.archGlass.refl_g = agm->GetKrefl().g;
//				gpum->param.archGlass.refl_b = agm->GetKrefl().b;
//
//				gpum->param.archGlass.refrct_r = agm->GetKrefrct().r;
//				gpum->param.archGlass.refrct_g = agm->GetKrefrct().g;
//				gpum->param.archGlass.refrct_b = agm->GetKrefrct().b;
//
//				gpum->param.archGlass.transFilter = agm->GetTransFilter();
//				gpum->param.archGlass.totFilter = agm->GetTotFilter();
//				gpum->param.archGlass.reflPdf = agm->GetReflPdf();
//				gpum->param.archGlass.transPdf = agm->GetTransPdf();
//				break;
//			}
//			default: {
//				enable_MAT_MATTE = true;
//				gpum->type = MAT_MATTE;
//				gpum->param.matte.r = 0.75f;
//				gpum->param.matte.g = 0.75f;
//				gpum->param.matte.b = 0.75f;
//				break;
//			}
//		}
//	}
//
//
//	//--------------------------------------------------------------------------
//	// Translate mesh material indices
//	//--------------------------------------------------------------------------
//
//	const u_int meshCount = scene->objectMaterials.size();
//	meshMats.resize(meshCount);
//	for (u_int i = 0; i < meshCount; ++i) {
//		Material *m = scene->objectMaterials[i];
//
//		// Look for the index
//		u_int index = 0;
//		for (u_int j = 0; j < materialsCount; ++j) {
//			if (m == scene->materials[j]) {
//				index = j;
//				break;
//			}
//		}
//
//		meshMats[i] = index;
//	}
//
//	const double tEnd = WallClockTime();
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Material compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::CompileAreaLights() {
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile AreaLights");
//
//	//--------------------------------------------------------------------------
//	// Translate area lights
//	//--------------------------------------------------------------------------
//
//	const double tStart = WallClockTime();
//
//	// Count the area lights
//	u_int areaLightCount = 0;
//	for (u_int i = 0; i < scene->lights.size(); ++i) {
//		if (scene->lights[i]->IsAreaLight())
//			++areaLightCount;
//	}
//
//	areaLights.resize(areaLightCount);
//	if (areaLightCount > 0) {
//		u_int index = 0;
//		for (u_int i = 0; i < scene->lights.size(); ++i) {
//			if (scene->lights[i]->IsAreaLight()) {
//				const TriangleLight *tl = (TriangleLight *)scene->lights[i];
//				const ExtMesh *mesh = scene->objects[tl->GetMeshIndex()];
//				const Triangle *tri = &(mesh->GetTriangles()[tl->GetTriIndex()]);
//
//				PathOCL::TriangleLight *cpl = &areaLights[index];
//				cpl->v0 = mesh->GetVertex(tri->v[0]);
//				cpl->v1 = mesh->GetVertex(tri->v[1]);
//				cpl->v2 = mesh->GetVertex(tri->v[2]);
//
//				cpl->normal = mesh->GetGeometryNormal(tl->GetTriIndex());
//
//				cpl->area = tl->GetArea();
//
//				AreaLightMaterial *alm = (AreaLightMaterial *)tl->GetMaterial();
//				cpl->gain_r = alm->GetGain().r;
//				cpl->gain_g = alm->GetGain().g;
//				cpl->gain_b = alm->GetGain().b;
//
//				++index;
//			}
//		}
//	}
//
//	const double tEnd = WallClockTime();
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Area lights compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

}

void CompiledScene::CompileInfiniteLight() {
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile InfiniteLight");
//
//	delete infiniteLight;
//
//	//--------------------------------------------------------------------------
//	// Check if there is an infinite light source
//	//--------------------------------------------------------------------------
//
//	const double tStart = WallClockTime();
//
//	InfiniteLight *il = (InfiniteLight *)scene->GetLightByType(TYPE_IL);
//	if (il) {
//		infiniteLight = new PathOCL::InfiniteLight();
//
//		infiniteLight->gain = il->GetGain();
//		infiniteLight->shiftU = il->GetShiftU();
//		infiniteLight->shiftV = il->GetShiftV();
//		const TextureMap *texMap = il->GetTexture()->GetTexMap();
//		infiniteLight->width = texMap->GetWidth();
//		infiniteLight->height = texMap->GetHeight();
//
//		infiniteLightMap = texMap->GetPixels();
//	} else {
//		infiniteLight = NULL;
//		infiniteLightMap = NULL;
//	}
//
//	const double tEnd = WallClockTime();
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Infinitelight compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::CompileSunLight() {
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile SunLight");
//
//	delete sunLight;
//
//	//--------------------------------------------------------------------------
//	// Check if there is an sun light source
//	//--------------------------------------------------------------------------
//
//	SunLight *sl = (SunLight *)scene->GetLightByType(TYPE_SUN);
//	if (sl) {
//		sunLight = new PathOCL::SunLight();
//
//		sunLight->sundir = sl->GetDir();
//		sunLight->gain = sl->GetGain();
//		sunLight->turbidity = sl->GetTubidity();
//		sunLight->relSize= sl->GetRelSize();
//		float tmp;
//		sl->GetInitData(&sunLight->x, &sunLight->y, &tmp, &tmp, &tmp,
//				&sunLight->cosThetaMax, &tmp, &sunLight->suncolor);
//	} else
//		sunLight = NULL;
}

void CompiledScene::CompileSkyLight() {
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile SkyLight");
//
//	delete skyLight;
//
//	//--------------------------------------------------------------------------
//	// Check if there is an sky light source
//	//--------------------------------------------------------------------------
//
//	SkyLight *sl = (SkyLight *)scene->GetLightByType(TYPE_IL_SKY);
//	if (sl) {
//		skyLight = new PathOCL::SkyLight();
//
//		skyLight->gain = sl->GetGain();
//		sl->GetInitData(&skyLight->thetaS, &skyLight->phiS,
//				&skyLight->zenith_Y, &skyLight->zenith_x, &skyLight->zenith_y,
//				skyLight->perez_Y, skyLight->perez_x, skyLight->perez_y);
//	} else
//		skyLight = NULL;
}

void CompiledScene::CompileTextureMaps() {
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile TextureMaps");
//
//	gpuTexMaps.resize(0);
//	rgbTexMemBlocks.resize(0);
//	alphaTexMemBlocks.resize(0);
//
//	meshTexMaps.resize(0);
//	meshTexMapsInfo.resize(0);
//	meshBumpMaps.resize(0);
//	meshBumpMapsInfo.resize(0);
//	meshNormalMaps.resize(0);
//	meshNormalMapsInfo.resize(0);
//
//	//--------------------------------------------------------------------------
//	// Translate mesh texture maps
//	//--------------------------------------------------------------------------
//
//	const double tStart = WallClockTime();
//
//	std::vector<TextureMap *> tms;
//	scene->texMapCache->GetTexMaps(tms);
//	// Calculate the amount of ram to allocate
//	totRGBTexMem = 0;
//	totAlphaTexMem = 0;
//
//	for (u_int i = 0; i < tms.size(); ++i) {
//		TextureMap *tm = tms[i];
//		const u_int pixelCount = tm->GetWidth() * tm->GetHeight();
//
//		totRGBTexMem += pixelCount;
//		if (tm->HasAlpha())
//			totAlphaTexMem += pixelCount;
//	}
//
//	// Allocate texture map memory
//	if ((totRGBTexMem > 0) || (totAlphaTexMem > 0)) {
//		gpuTexMaps.resize(tms.size());
//
//		if (totRGBTexMem > 0) {
//			rgbTexMemBlocks.resize(1);
//
//			for (u_int i = 0; i < tms.size(); ++i) {
//				TextureMap *tm = tms[i];
//				const u_int pixelCount = tm->GetWidth() * tm->GetHeight();
//				const u_int texSize = pixelCount * sizeof(Spectrum);
//
//				if (texSize > maxMemPageSize)
//					throw std::runtime_error("The RGB channels of a texture map are too big to fit in a single block of memory");
//
//				bool found = false;
//				u_int page;
//				for (u_int j = 0; j < rgbTexMemBlocks.size(); ++j) {
//					// Check if it fits in the this page
//					if (texSize + rgbTexMemBlocks[j].size() * sizeof(Spectrum) <= maxMemPageSize) {
//						found = true;
//						page = j;
//						break;
//					}
//				}
//
//				if (!found) {
//					// Check if I can add a new page
//					if (rgbTexMemBlocks.size() > 5)
//						throw std::runtime_error("More than 5 blocks of memory are required for RGB channels of a texture maps");
//
//					// Add a new page
//					rgbTexMemBlocks.push_back(vector<Spectrum>());
//					page = rgbTexMemBlocks.size() - 1;
//				}
//
//				gpuTexMaps[i].rgbPage = page;
//				gpuTexMaps[i].rgbPageOffset = (u_int)rgbTexMemBlocks[page].size();
//				rgbTexMemBlocks[page].insert(rgbTexMemBlocks[page].end(), tm->GetPixels(), tm->GetPixels() + pixelCount);
//			}
//		}
//
//		if (totAlphaTexMem > 0) {
//			alphaTexMemBlocks.resize(1);
//
//			for (u_int i = 0; i < tms.size(); ++i) {
//				TextureMap *tm = tms[i];
//				
//				if (tm->HasAlpha()) {
//					const u_int pixelCount = tm->GetWidth() * tm->GetHeight();
//					const u_int texSize = pixelCount * sizeof(float);
//
//					if (texSize > maxMemPageSize)
//						throw std::runtime_error("The alpha channel of a texture map is too big to fit in a single block of memory");
//
//					bool found = false;
//					u_int page;
//					for (u_int j = 0; j < alphaTexMemBlocks.size(); ++j) {
//						// Check if it fits in the this page
//						if (texSize + alphaTexMemBlocks[j].size() * sizeof(float) <= maxMemPageSize) {
//							found = true;
//							page = j;
//							break;
//						}
//					}
//
//					if (!found) {
//						// Check if I can add a new page
//						if (alphaTexMemBlocks.size() > 5)
//							throw std::runtime_error("More than 5 blocks of memory are required for alpha channels of a texture maps");
//
//						// Add a new page
//						alphaTexMemBlocks.push_back(vector<float>());
//						page = alphaTexMemBlocks.size() - 1;
//					}
//
//					gpuTexMaps[i].alphaPage = page;
//					gpuTexMaps[i].alphaPageOffset = (u_int)alphaTexMemBlocks[page].size();
//					alphaTexMemBlocks[page].insert(alphaTexMemBlocks[page].end(), tm->GetAlphas(), tm->GetAlphas() + pixelCount);
//				} else {
//					gpuTexMaps[i].alphaPage = 0xffffffffu;
//					gpuTexMaps[i].alphaPageOffset = 0xffffffffu;
//				}
//			}
//		}
//
//		//----------------------------------------------------------------------
//
//		// Translate texture map description
//		for (u_int i = 0; i < tms.size(); ++i) {
//			TextureMap *tm = tms[i];
//			gpuTexMaps[i].width = tm->GetWidth();
//			gpuTexMaps[i].height = tm->GetHeight();
//		}
//
//		//----------------------------------------------------------------------
//
//		// Translate mesh texture indices
//		const u_int meshCount = meshMats.size();
//		meshTexMaps.resize(meshCount);
//		meshTexMapsInfo.resize(meshCount);
//		for (u_int i = 0; i < meshCount; ++i) {
//			TexMapInstance *t = scene->objectTexMaps[i];
//
//			if (t) {
//				// Look for the index
//				u_int index = 0;
//				for (u_int j = 0; j < tms.size(); ++j) {
//					if (t->GetTexMap() == tms[j]) {
//						index = j;
//						break;
//					}
//				}
//
//				meshTexMaps[i] = index;
//				meshTexMapsInfo[i].uScale = t->GetUScale();
//				meshTexMapsInfo[i].vScale = t->GetVScale();
//				meshTexMapsInfo[i].uDelta = t->GetUDelta();
//				meshTexMapsInfo[i].vDelta = t->GetVDelta();
//			} else {
//				meshTexMaps[i] = 0xffffffffu;
//				meshTexMapsInfo[i].uScale = 1.f;
//				meshTexMapsInfo[i].vScale = 1.f;
//				meshTexMapsInfo[i].uDelta = 0.f;
//				meshTexMapsInfo[i].vDelta = 0.f;
//			}
//		}
//
//		//----------------------------------------------------------------------
//
//		// Translate mesh bump map indices
//		bool hasBumpMapping = false;
//		meshBumpMaps.resize(meshCount);
//		meshBumpMapsInfo.resize(meshCount);
//		for (u_int i = 0; i < meshCount; ++i) {
//			BumpMapInstance *bm = scene->objectBumpMaps[i];
//
//			if (bm) {
//				// Look for the index
//				u_int index = 0;
//				for (u_int j = 0; j < tms.size(); ++j) {
//					if (bm->GetTexMap() == tms[j]) {
//						index = j;
//						break;
//					}
//				}
//
//				meshBumpMaps[i] = index;
//				meshBumpMapsInfo[i].uScale = bm->GetUScale();
//				meshBumpMapsInfo[i].uScale = bm->GetUScale();
//				meshBumpMapsInfo[i].vScale = bm->GetVScale();
//				meshBumpMapsInfo[i].uDelta = bm->GetUDelta();
//				meshBumpMapsInfo[i].vDelta = bm->GetVDelta();
//				meshBumpMapsInfo[i].scale = bm->GetScale();
//
//				hasBumpMapping = true;
//			} else
//				meshBumpMaps[i] = 0xffffffffu;
//		}
//
//		if (!hasBumpMapping) {
//			meshBumpMaps.resize(0);
//			meshBumpMapsInfo.resize(0);
//		}
//
//		//----------------------------------------------------------------------
//
//		// Translate mesh normal map indices
//		bool hasNormalMapping = false;
//		meshNormalMaps.resize(meshCount);
//		meshNormalMapsInfo.resize(meshCount);
//		for (u_int i = 0; i < meshCount; ++i) {
//			NormalMapInstance *nm = scene->objectNormalMaps[i];
//
//			if (nm) {
//				// Look for the index
//				u_int index = 0;
//				for (u_int j = 0; j < tms.size(); ++j) {
//					if (nm->GetTexMap() == tms[j]) {
//						index = j;
//						break;
//					}
//				}
//
//				meshNormalMaps[i] = index;
//				meshNormalMapsInfo[i].uScale = nm->GetUScale();
//				meshNormalMapsInfo[i].uScale = nm->GetUScale();
//				meshNormalMapsInfo[i].vScale = nm->GetVScale();
//				meshNormalMapsInfo[i].uDelta = nm->GetUDelta();
//				meshNormalMapsInfo[i].vDelta = nm->GetVDelta();
//
//				hasNormalMapping = true;
//			} else
//				meshNormalMaps[i] = 0xffffffffu;
//		}
//
//		if (!hasNormalMapping) {
//			meshNormalMaps.resize(0);
//			meshNormalMapsInfo.resize(0);
//		}
//	} else {
//		gpuTexMaps.resize(0);
//		rgbTexMemBlocks.resize(0);
//		alphaTexMemBlocks.resize(0);
//		meshTexMaps.resize(0);
//		meshTexMapsInfo.resize(0);
//		meshBumpMaps.resize(0);
//		meshBumpMapsInfo.resize(0);
//		meshNormalMaps.resize(0);
//		meshNormalMapsInfo.resize(0);
//	}
//
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Texture maps RGB channel page count: " << rgbTexMemBlocks.size());
//	for (u_int i = 0; i < rgbTexMemBlocks.size(); ++i)
//		SLG_LOG("[PathOCLRenderThread::CompiledScene]  RGB channel page " << i << " size: " << rgbTexMemBlocks[i].size() * sizeof(Spectrum) / 1024 << "Kbytes");
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Texture maps Alpha channel page count: " << alphaTexMemBlocks.size());
//	for (u_int i = 0; i < alphaTexMemBlocks.size(); ++i)
//		SLG_LOG("[PathOCLRenderThread::CompiledScene]  Alpha channel page " << i << " size: " << alphaTexMemBlocks[i].size() * sizeof(float) / 1024 << "Kbytes");
//
//	const double tEnd = WallClockTime();
//	SLG_LOG("[PathOCLRenderThread::CompiledScene] Texture maps compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::Recompile(const EditActionList &editActions) {
	if (editActions.Has(FILM_EDIT) || editActions.Has(CAMERA_EDIT))
		CompileCamera();
	if (editActions.Has(GEOMETRY_EDIT))
		CompileGeometry();
	if (editActions.Has(MATERIALS_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT))
		CompileMaterials();
	if (editActions.Has(AREALIGHTS_EDIT))
		CompileAreaLights();
	if (editActions.Has(INFINITELIGHT_EDIT))
		CompileInfiniteLight();
	if (editActions.Has(SUNLIGHT_EDIT))
		CompileSunLight();
	if (editActions.Has(SKYLIGHT_EDIT))
		CompileSkyLight();
	if (editActions.Has(TEXTUREMAPS_EDIT))
		CompileTextureMaps();
}

bool CompiledScene::IsMaterialCompiled(const MaterialType type) const {
//	switch (type) {
//		case MATTE:
//			return enable_MAT_MATTE;
//		case AREALIGHT:
//			return enable_MAT_AREALIGHT;
//		case MIRROR:
//			return enable_MAT_MIRROR;
//		case MATTEMIRROR:
//			return enable_MAT_MATTEMIRROR;
//		case GLASS:
//			return enable_MAT_GLASS;
//		case METAL:
//			return enable_MAT_METAL;
//		case MATTEMETAL:
//			return enable_MAT_MATTEMETAL;
//		case ARCHGLASS:
//			return enable_MAT_ARCHGLASS;
//		case ALLOY:
//			return enable_MAT_ALLOY;
//		default:
//			assert (false);
//			return false;
//			break;
//	}

	return true;
}

}

#endif
