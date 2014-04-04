#line 2 "biaspatchocl_kernels.cl"

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

// List of symbols defined at compile time:
//  PARAM_TASK_COUNT
//  PARAM_TILE_SIZE
//  PARAM_DIRECT_LIGHT_ONE_STRATEGY or PARAM_DIRECT_LIGHT_ALL_STRATEGY
//  PARAM_RADIANCE_CLAMP_MAXVALUE
//  PARAM_PDF_CLAMP_VALUE
//  PARAM_AA_SAMPLES
//  PARAM_DIRECT_LIGHT_SAMPLES
//  PARAM_DIFFUSE_SAMPLES
//  PARAM_GLOSSY_SAMPLES
//  PARAM_SPECULAR_SAMPLES
//  PARAM_DEPTH_MAX
//  PARAM_DEPTH_DIFFUSE_MAX
//  PARAM_DEPTH_GLOSSY_MAX
//  PARAM_DEPTH_SPECULAR_MAX
//  PARAM_IMAGE_FILTER_WIDTH
//  PARAM_LOW_LIGHT_THREASHOLD
//  PARAM_NEAR_START_LIGHT

//------------------------------------------------------------------------------
// InitSeed Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitSeed(
		uint seedBase,
		__global GPUTask *tasks) {
	//if (get_global_id(0) == 0)
	//	printf("sizeof(BSDF) = %dbytes\n", sizeof(BSDF));
	//if (get_global_id(0) == 0)
	//	printf("sizeof(HitPoint) = %dbytes\n", sizeof(HitPoint));
	//if (get_global_id(0) == 0)
	//	printf("sizeof(GPUTask) = %dbytes\n", sizeof(GPUTask));
	//if (get_global_id(0) == 0)
	//	printf("sizeof(SampleResult) = %dbytes\n", sizeof(SampleResult));

	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Initialize the task
	__global GPUTask *task = &tasks[gid];

	// For testing/debugging
	//MangleMemory((__global unsigned char *)task, sizeof(GPUTask));

	// Initialize random number generator
	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------
// InitStats Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitStat(
		__global GPUTaskStats *taskStats) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	__global GPUTaskStats *taskStat = &taskStats[gid];
	taskStat->raysCount = 0;
}

//------------------------------------------------------------------------------
// RenderSample Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample(
		const uint tileStartX,
		const uint tileStartY,
		const int tileSampleIndex,
		const uint engineFilmWidth, const uint engineFilmHeight,
		__global GPUTask *tasks,
		__global GPUTaskDirectLight *tasksDirectLight,
		__global GPUTaskPathVertexN *tasksPathVertexN,
		__global GPUTaskStats *taskStats,
		__global SampleResult *taskResults,
		__global float *pixelFilterDistribution,
		// Film parameters
		const uint filmWidth, const uint filmHeight
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
		, __global float *filmRadianceGroup0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
		, __global float *filmRadianceGroup1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
		, __global float *filmRadianceGroup2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
		, __global float *filmRadianceGroup3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
		, __global float *filmRadianceGroup4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
		, __global float *filmRadianceGroup5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
		, __global float *filmRadianceGroup6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
		, __global float *filmRadianceGroup7
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
		, __global float *filmAlpha
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
		, __global float *filmDepth
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		, __global float *filmPosition
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		, __global float *filmGeometryNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		, __global float *filmShadingNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		, __global uint *filmMaterialID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
		, __global float *filmDirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
		, __global float *filmDirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
		, __global float *filmEmission
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
		, __global float *filmIndirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
		, __global float *filmIndirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
		, __global float *filmIndirectSpecular
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
		, __global float *filmMaterialIDMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
		, __global float *filmDirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		, __global float *filmIndirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		, __global float *filmUV
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
		, __global float *filmRayCount
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
		, __global float *filmByMaterialID
#endif
		,
		// Scene parameters
#if defined(PARAM_HAS_INFINITELIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
		__global Material *mats,
		__global Texture *texs,
		__global uint *meshMats,
		__global Mesh *meshDescs,
		__global Point *vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
		__global Vector *vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
		__global UV *vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
		__global Spectrum *vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
		__global float *vertAlphas,
#endif
		__global Triangle *triangles,
		__global Camera *camera,
		// Lights
		__global LightSource *lights,
#if defined(PARAM_HAS_ENVLIGHTS)
		__global uint *envLightIndices,
		const uint envLightCount,
#endif
		__global uint *meshTriLightDefsOffset,
#if defined(PARAM_HAS_INFINITELIGHT)
		__global float *infiniteLightDistribution,
#endif
		__global float *lightsDistribution
		// Images
#if defined(PARAM_IMAGEMAPS_PAGE_0)
		, __global ImageMap *imageMapDescs, __global float *imageMapBuff0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
		, __global float *imageMapBuff1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
		, __global float *imageMapBuff2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
		, __global float *imageMapBuff3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
		, __global float *imageMapBuff4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
		, __global float *imageMapBuff5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
		, __global float *imageMapBuff6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
		, __global float *imageMapBuff7
#endif
		ACCELERATOR_INTERSECT_PARAM_DECL
		) {
	const size_t gid = get_global_id(0);

	const uint sampleIndex = gid % (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelIndex = gid / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelX = samplePixelIndex % PARAM_TILE_SIZE;
	const uint samplePixelY = samplePixelIndex / PARAM_TILE_SIZE;

	if ((gid >= PARAM_TILE_SIZE * PARAM_TILE_SIZE * PARAM_AA_SAMPLES * PARAM_AA_SAMPLES) ||
			(tileStartX + samplePixelX >= engineFilmWidth) ||
			(tileStartY + samplePixelY >= engineFilmHeight))
		return;

	__global GPUTask *task = &tasks[gid];
	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];
	__global GPUTaskPathVertexN *taskPathVertexN = &tasksPathVertexN[gid];
	__global GPUTaskStats *taskStat = &taskStats[gid];

	//--------------------------------------------------------------------------
	// Initialize image maps page pointer table
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)
	__global float *imageMapBuff[PARAM_IMAGEMAPS_COUNT];
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	imageMapBuff[0] = imageMapBuff0;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	imageMapBuff[1] = imageMapBuff1;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	imageMapBuff[2] = imageMapBuff2;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	imageMapBuff[3] = imageMapBuff3;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	imageMapBuff[4] = imageMapBuff4;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
	imageMapBuff[5] = imageMapBuff5;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
	imageMapBuff[6] = imageMapBuff6;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
	imageMapBuff[7] = imageMapBuff7;
#endif
#endif

	//--------------------------------------------------------------------------
	// Initialize the first ray
	//--------------------------------------------------------------------------

	// Read the seed
	Seed seed;
	seed.s1 = task->seed.s1;
	seed.s2 = task->seed.s2;
	seed.s3 = task->seed.s3;

	__global SampleResult *sampleResult = &taskResults[gid];
	SampleResult_Init(sampleResult);
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	sampleResult->directShadowMask = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	sampleResult->indirectShadowMask = 0.f;
#endif

	Ray ray;
	RayHit rayHit;
	GenerateCameraRay(&seed, task, sampleResult,
			camera, pixelFilterDistribution,
			samplePixelX, samplePixelY, sampleIndex,
			tileStartX, tileStartY, 
			engineFilmWidth, engineFilmHeight, &ray);

	//--------------------------------------------------------------------------
	// Render a sample
	//--------------------------------------------------------------------------

	taskPathVertexN->vertex1SampleComponent = DIFFUSE;
	taskPathVertexN->vertex1SampleIndex = 0;

	uint tracedRaysCount = taskStat->raysCount;
	uint pathState = PATH_VERTEX_1 | NEXT_VERTEX_TRACE_RAY;
	PathDepthInfo depthInfo;
	PathDepthInfo_Init(&depthInfo, 0);
	float lastPdfW = 1.f;
	BSDFEvent pathBSDFEvent = NONE;
	BSDFEvent lastBSDFEvent = SPECULAR;

	__global BSDF *currentBSDF = &task->bsdfPathVertex1;
	__global Spectrum *currentThroughput = &task->throughputPathVertex1;
	VSTORE3F(WHITE, task->throughputPathVertex1.c);
#if defined(PARAM_HAS_PASSTHROUGH)
	// This is a bit tricky. I store the passThroughEvent in the BSDF
	// before of the initialization because it can be used during the
	// tracing of next path vertex ray.

	task->bsdfPathVertex1.hitPoint.passThroughEvent = Rnd_FloatValue(&seed);
#endif

	//if (get_global_id(0) == 0) {
	//	printf("============================================================\n");
	//	printf("== Begin loop\n");
	//	printf("==\n");
	//	printf("== task->bsdfPathVertex1: %x\n", &task->bsdfPathVertex1);
	//	printf("== taskDirectLight->directLightBSDF: %x\n", &taskDirectLight->directLightBSDF);
	//	printf("== taskPathVertexN->bsdfPathVertexN: %x\n", &taskPathVertexN->bsdfPathVertexN);
	//	printf("============================================================\n");
	//}

	while (!(pathState & DONE)) {
		//if (get_global_id(0) == 0)
		//	printf("Depth: %d  [pathState: %d|%d][currentBSDF: %x][currentThroughput: %x]\n",
		//			depthInfo.depth, pathState >> 16, pathState & LOW_STATE_MASK, currentBSDF, currentThroughput);

		//----------------------------------------------------------------------
		// Ray trace step
		//----------------------------------------------------------------------

		Accelerator_Intersect(&ray, &rayHit
			ACCELERATOR_INTERSECT_PARAM);
		++tracedRaysCount;

		if (rayHit.meshIndex != NULL_INDEX) {
			// Something was hit, initialize the BSDF
			BSDF_Init(currentBSDF,
					meshDescs,
					meshMats,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
					meshTriLightDefsOffset,
#endif
					vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
					vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
					vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
					vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
					vertAlphas,
#endif
					triangles, &ray, &rayHit
#if defined(PARAM_HAS_PASSTHROUGH)
					, currentBSDF->hitPoint.passThroughEvent
#endif
#if defined(PARAM_HAS_BUMPMAPS)
					MATERIALS_PARAM
#endif
					);

#if defined(PARAM_HAS_PASSTHROUGH)
			const float3 passThroughTrans = BSDF_GetPassThroughTransparency(currentBSDF
					MATERIALS_PARAM);
			if (!Spectrum_IsBlack(passThroughTrans)) {
				const float3 pathThroughput = VLOAD3F(currentThroughput->c) * passThroughTrans;
				VSTORE3F(pathThroughput, currentThroughput->c);

				// It is a pass through point, continue to trace the ray
				ray.mint = rayHit.t + MachineEpsilon_E(rayHit.t);

				continue;
			}
#endif
		}

		//----------------------------------------------------------------------
		// Advance the finite state machine step
		//----------------------------------------------------------------------

		//----------------------------------------------------------------------
		// Evaluation of the finite state machine.
		//
		// From: * | NEXT_VERTEX_TRACE_RAY
		// To: DONE or
		//     (* | DIRECT_LIGHT_GENERATE_RAY)
		//     (PATH_VERTEX_N | NEXT_GENERATE_TRACE_RAY)
		//----------------------------------------------------------------------

		if (pathState & NEXT_VERTEX_TRACE_RAY) {
			const bool firstPathVertex = (pathState & PATH_VERTEX_1);

			if (rayHit.meshIndex != NULL_INDEX) {
				//--------------------------------------------------------------
				// Something was hit
				//--------------------------------------------------------------

				if (firstPathVertex) {
					// Save the path first vertex information
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
					sampleResult->alpha = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
					sampleResult->depth = rayHit.t;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
					sampleResult->position = task->bsdfPathVertex1.hitPoint.p;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
					sampleResult->geometryNormal = task->bsdfPathVertex1.hitPoint.geometryN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
					sampleResult->shadingNormal = task->bsdfPathVertex1.hitPoint.shadeN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
					sampleResult->materialID = BSDF_GetMaterialID(&task->bsdfPathVertex1
						MATERIALS_PARAM);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
					sampleResult->uv = task->bsdfPathVertex1.hitPoint.uv;
#endif
				}

				if (firstPathVertex || (mats[currentBSDF->materialIndex].visibility & (pathBSDFEvent & (DIFFUSE | GLOSSY | SPECULAR)))) {
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
					// Check if it is a light source (note: I can hit only triangle area light sources)
					if (BSDF_IsLightSource(currentBSDF) && (rayHit.t > PARAM_NEAR_START_LIGHT)) {
						DirectHitFiniteLight(firstPathVertex, lastBSDFEvent,
								pathBSDFEvent,
								currentThroughput,
								rayHit.t, currentBSDF, lastPdfW,
								sampleResult
								LIGHTS_PARAM);
					}
#endif
					// Before Direct Lighting in order to have a correct MIS
					if (PathDepthInfo_CheckDepths(&depthInfo)) {
#if defined(PARAM_DIRECT_LIGHT_ALL_STRATEGY)
						taskDirectLight->lightIndex = 0;
						taskDirectLight->lightSampleIndex = 0;
#endif
						pathState = (pathState & HIGH_STATE_MASK) | DIRECT_LIGHT_GENERATE_RAY;
					} else {
						pathState = firstPathVertex ? DONE :
							(PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY);
					}
				} else
					pathState = PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY;
			} else {
				//--------------------------------------------------------------
				// Nothing was hit, add environmental lights radiance
				//--------------------------------------------------------------

#if defined(PARAM_HAS_ENVLIGHTS)
				const float3 rayDir = (float3)(ray.d.x, ray.d.y, ray.d.z);
				DirectHitInfiniteLight(
						firstPathVertex,
						lastBSDFEvent,
						pathBSDFEvent,
						currentThroughput,
						-rayDir, lastPdfW,
						sampleResult
						LIGHTS_PARAM);
#endif

				if (firstPathVertex) {
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
					sampleResult->alpha = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
					sampleResult->depth = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
					sampleResult->position.x = INFINITY;
					sampleResult->position.y = INFINITY;
					sampleResult->position.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
					sampleResult->geometryNormal.x = INFINITY;
					sampleResult->geometryNormal.y = INFINITY;
					sampleResult->geometryNormal.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
					sampleResult->shadingNormal.x = INFINITY;
					sampleResult->shadingNormal.y = INFINITY;
					sampleResult->shadingNormal.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
					sampleResult->materialID = NULL_INDEX;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
					sampleResult->uv.u = INFINITY;
					sampleResult->uv.v = INFINITY;
#endif
				}

				pathState = firstPathVertex ? DONE : (PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY);
			}
		}

		//----------------------------------------------------------------------
		// Evaluation of the finite state machine.
		//
		// From: * | DIRECT_LIGHT_TRACE_RAY
		// To: (* | NEXT_VERTEX_GENERATE_RAY) or
		//     (* | DIRECT_LIGHT_GENERATE_RAY)
		//----------------------------------------------------------------------

		if (pathState & DIRECT_LIGHT_TRACE_RAY) {
			const bool firstPathVertex = (pathState & PATH_VERTEX_1);

			if (rayHit.meshIndex == NULL_INDEX) {
				//--------------------------------------------------------------
				// Nothing was hit, the light source is visible
				//--------------------------------------------------------------

				// currentThroughput contains the shadow ray throughput
				const float3 lightRadiance = VLOAD3F(currentThroughput->c) * VLOAD3F(taskDirectLight->lightRadiance.c);
				const uint lightID = taskDirectLight->lightID;
				VADD3F(sampleResult->radiancePerPixelNormalized[lightID].c, lightRadiance);

				//if (get_global_id(0) == 0)
				//	printf("DIRECT_LIGHT_TRACE_RAY => lightRadiance: %f %f %f [%d]\n", lightRadiance.s0, lightRadiance.s1, lightRadiance.s2, lightID);

				if (firstPathVertex) {
					if (BSDF_GetEventTypes(&task->bsdfPathVertex1
							MATERIALS_PARAM) & DIFFUSE) {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
						VADD3F(&sampleResult->directDiffuse.r, lightRadiance);
#endif
					} else {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
						VADD3F(&sampleResult->directGlossy.r, lightRadiance);
#endif
					}
				} else {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
					sampleResult->indirectShadowMask = 0.f;
#endif

					if (pathBSDFEvent & DIFFUSE) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
						VADD3F(sampleResult->indirectDiffuse.c, lightRadiance);
#endif
					} else if (pathBSDFEvent & GLOSSY) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
						VADD3F(sampleResult->indirectGlossy.c, lightRadiance);
#endif
					} else if (pathBSDFEvent & SPECULAR) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
						VADD3F(sampleResult->indirectSpecular.c, lightRadiance);
#endif
					}
				}
			}
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
			else {
				if (firstPathVertex) {
					const int lightSamplesCount = lightSamples[taskDirectLight->lightIndex];
					const uint sampleCount = (lightSamplesCount < 0) ? PARAM_DIRECT_LIGHT_SAMPLES : (uint)lightSamplesCount;

					sampleResult->directShadowMask += 1.f / (sampleCount * sampleCount);
				}
			}
#endif

#if defined(PARAM_DIRECT_LIGHT_ALL_STRATEGY)
			if (firstPathVertex) {
				const uint lightIndex = taskDirectLight->lightIndex;
				if (lightIndex <= PARAM_LIGHT_COUNT) {
					++(taskDirectLight->lightSampleIndex);
					pathState = PATH_VERTEX_1 | DIRECT_LIGHT_GENERATE_RAY;
				} else {
					// Move to the next path vertex
					pathState = PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY;
				}
			} else {
				// Move to the next path vertex
				pathState = PATH_VERTEX_N | NEXT_VERTEX_GENERATE_RAY;
			}
#else
			// Move to the next path vertex
			pathState = (pathState & HIGH_STATE_MASK) | NEXT_VERTEX_GENERATE_RAY;
#endif
		}

		//----------------------------------------------------------------------
		// Evaluation of the finite state machine.
		//
		// From: * | DIRECT_LIGHT_GENERATE_RAY
		// To: (* | NEXT_VERTEX_GENERATE_RAY) or 
		//     (* | DIRECT_LIGHT_TRACE_RAY[continue])
		//     (* | NEXT_VERTEX_GENERATE_RAY)
		//----------------------------------------------------------------------

		if (pathState & DIRECT_LIGHT_GENERATE_RAY) {
			const bool firstPathVertex = (pathState & PATH_VERTEX_1);

			if (BSDF_IsDelta(firstPathVertex ? &task->bsdfPathVertex1 : &taskPathVertexN->bsdfPathVertexN
				MATERIALS_PARAM)) {
				// Move to the next path vertex
				pathState = (pathState & HIGH_STATE_MASK) | NEXT_VERTEX_GENERATE_RAY;
			} else {
				const bool illuminated =
#if defined(PARAM_DIRECT_LIGHT_ALL_STRATEGY)
				(!firstPathVertex) ?
#endif
					DirectLightSampling_ONE(
						firstPathVertex,
						&seed,
#if defined(PARAM_HAS_INFINITELIGHTS)
						worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
						&taskDirectLight->directLightHitPoint,
#endif
						firstPathVertex ? &task->throughputPathVertex1 : &taskPathVertexN->throughputPathVertexN,
						firstPathVertex ? &task->bsdfPathVertex1 : &taskPathVertexN->bsdfPathVertexN,
						sampleResult,
						&ray, &taskDirectLight->lightRadiance, &taskDirectLight->lightID
						LIGHTS_PARAM)
#if defined(PARAM_DIRECT_LIGHT_ALL_STRATEGY)
				: DirectLightSampling_ALL(
						&taskDirectLight->lightIndex,
						&taskDirectLight->lightSampleIndex,
						&seed,
#if defined(PARAM_HAS_INFINITELIGHTS)
						worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
						&taskDirectLight->directLightHitPoint,
#endif
						&task->throughputPathVertex1, &task->bsdfPathVertex1,
						sampleResult,
						&ray, &taskDirectLight->lightRadiance, &taskDirectLight->lightID
						LIGHTS_PARAM)
#endif
				;
				
				if (illuminated) {
					// Trace the shadow ray
					currentBSDF = &taskDirectLight->directLightBSDF;
					currentThroughput = &taskDirectLight->directLightThroughput;
					VSTORE3F(WHITE, currentThroughput->c);
#if defined(PARAM_HAS_PASSTHROUGH)
					// This is a bit tricky. I store the passThroughEvent in the BSDF
					// before of the initialization because it can be use during the
					// tracing of next path vertex ray.
					taskDirectLight->directLightBSDF.hitPoint.passThroughEvent = Rnd_FloatValue(&seed);
#endif

					pathState = (pathState & HIGH_STATE_MASK) | DIRECT_LIGHT_TRACE_RAY;
				} else {
					// Move to the next path vertex
					pathState = (pathState & HIGH_STATE_MASK) | NEXT_VERTEX_GENERATE_RAY;
				}
			}
		}

		//----------------------------------------------------------------------
		// Evaluation of the finite state machine.
		//
		// From: PATH_VERTEX_N | NEXT_VERTEX_GENERATE_RAY
		// To: PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY or (PATH_VERTEX_N | NEXT_VERTEX_TRACE_RAY[continue])
		//----------------------------------------------------------------------

		if (pathState == (PATH_VERTEX_N | NEXT_VERTEX_GENERATE_RAY)) {
			//if (get_global_id(0) == 0)
			//	printf("(PATH_VERTEX_N | NEXT_VERTEX_GENERATE_RAY)) ==> Depth: %d  [pathState: %d|%d][currentThroughput: %f %f %f]\n",
			//			depthInfo.depth, pathState >> 16, pathState & LOW_STATE_MASK, currentThroughput->r, currentThroughput->g, currentThroughput->b);

			// Sample the BSDF
			float3 sampledDir;
			float cosSampledDir;
			BSDFEvent event;
			const float3 bsdfSample = BSDF_Sample(&taskPathVertexN->bsdfPathVertexN,
				Rnd_FloatValue(&seed),
				Rnd_FloatValue(&seed),
				&sampledDir, &lastPdfW, &cosSampledDir, &event, ALL
				MATERIALS_PARAM);

			PathDepthInfo_IncDepths(&depthInfo, event);

			if (!Spectrum_IsBlack(bsdfSample)) {
				float3 throughput = VLOAD3F(taskPathVertexN->throughputPathVertexN.c);
				throughput *= bsdfSample * ((event & SPECULAR) ? 1.f : min(1.f, lastPdfW / PARAM_PDF_CLAMP_VALUE));
				VSTORE3F(throughput, taskPathVertexN->throughputPathVertexN.c);

				Ray_Init2_Private(&ray, VLOAD3F(&taskPathVertexN->bsdfPathVertexN.hitPoint.p.x), sampledDir);

				lastBSDFEvent = event;

#if defined(PARAM_HAS_PASSTHROUGH)
				// This is a bit tricky. I store the passThroughEvent in the BSDF
				// before of the initialization because it can be use during the
				// tracing of next path vertex ray.
				taskPathVertexN->bsdfPathVertexN.hitPoint.passThroughEvent = Rnd_FloatValue(&seed);
#endif
				currentBSDF = &taskPathVertexN->bsdfPathVertexN;
				currentThroughput = &taskPathVertexN->throughputPathVertexN;

				pathState = PATH_VERTEX_N | NEXT_VERTEX_TRACE_RAY;
			} else
				pathState = PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY;
		}

		//----------------------------------------------------------------------
		// Evaluation of the finite state machine.
		//
		// From: PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY
		// To: DONE or (PATH_VERTEX_N | NEXT_VERTEX_TRACE_RAY[continue])
		//----------------------------------------------------------------------

		if (pathState == (PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY)) {
			//if (get_global_id(0) == 0)
			//	printf("(PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY)) ==> Depth: %d  [pathState: %d|%d][currentThroughput: %f %f %f]\n",
			//			depthInfo.depth, pathState >> 16, pathState & LOW_STATE_MASK, currentThroughput->r, currentThroughput->g, currentThroughput->b);

			BSDFEvent vertex1SampleComponent = taskPathVertexN->vertex1SampleComponent;
			uint vertex1SampleIndex = taskPathVertexN->vertex1SampleIndex;
			const BSDFEvent materialEventTypes = BSDF_GetEventTypes(&task->bsdfPathVertex1
				MATERIALS_PARAM);

			for (;;) {
				const int matSamplesCount = mats[task->bsdfPathVertex1.materialIndex].samples;
				const uint globalMatSamplesCount = ((vertex1SampleComponent == DIFFUSE) ? PARAM_DIFFUSE_SAMPLES :
					((vertex1SampleComponent == GLOSSY) ? PARAM_GLOSSY_SAMPLES :
						PARAM_SPECULAR_SAMPLES));
				const uint sampleCount = (matSamplesCount < 0) ? globalMatSamplesCount : (uint)matSamplesCount;
				const uint sampleCount2 = sampleCount * sampleCount;

				if (!(materialEventTypes & vertex1SampleComponent) || (vertex1SampleIndex >= sampleCount2)) {
					// Move to next component
					vertex1SampleComponent = (vertex1SampleComponent == DIFFUSE) ? GLOSSY :
						((vertex1SampleComponent == GLOSSY) ? SPECULAR : NONE);

					if (vertex1SampleComponent == NONE) {
						pathState = DONE;
						break;
					}

					vertex1SampleIndex = 0;
				} else {
					// Sample the BSDF
					float3 sampledDir;
					float cosSampledDir;
					BSDFEvent event;

					float u0, u1;
					SampleGrid(&seed, sampleCount,
						vertex1SampleIndex % sampleCount, vertex1SampleIndex / sampleCount,
						&u0, &u1);

					// Now, I can increment vertex1SampleIndex
					++vertex1SampleIndex;

#if defined(PARAM_HAS_PASSTHROUGH)
					// This is a bit tricky. I store the passThroughEvent in the BSDF
					// before of the initialization because it can be use during the
					// tracing of next path vertex ray.
					task->bsdfPathVertex1.hitPoint.passThroughEvent = Rnd_FloatValue(&seed);
#endif
					const float3 bsdfSample = BSDF_Sample(&task->bsdfPathVertex1,
							u0,
							u1,
							&sampledDir, &lastPdfW, &cosSampledDir, &event,
							vertex1SampleComponent | REFLECT | TRANSMIT
							MATERIALS_PARAM);

					PathDepthInfo_Init(&depthInfo, 0);
					PathDepthInfo_IncDepths(&depthInfo, event);

					if (!Spectrum_IsBlack(bsdfSample)) {
						const float scaleFactor = 1.f / sampleCount2;
						float3 throughput = VLOAD3F(task->throughputPathVertex1.c);
						throughput *= bsdfSample * (scaleFactor * ((event & SPECULAR) ? 1.f : min(1.f, lastPdfW / PARAM_PDF_CLAMP_VALUE)));
						VSTORE3F(throughput, taskPathVertexN->throughputPathVertexN.c);

						Ray_Init2_Private(&ray, VLOAD3F(&task->bsdfPathVertex1.hitPoint.p.x), sampledDir);

						pathBSDFEvent = event;
						lastBSDFEvent = event;

#if defined(PARAM_HAS_PASSTHROUGH)
						// This is a bit tricky. I store the passThroughEvent in the BSDF
						// before of the initialization because it can be use during the
						// tracing of next path vertex ray.
						taskPathVertexN->bsdfPathVertexN.hitPoint.passThroughEvent = Rnd_FloatValue(&seed);
#endif
						currentBSDF = &taskPathVertexN->bsdfPathVertexN;
						currentThroughput = &taskPathVertexN->throughputPathVertexN;

						pathState = PATH_VERTEX_N | NEXT_VERTEX_TRACE_RAY;

						// Save vertex1SampleComponent and vertex1SampleIndex
						taskPathVertexN->vertex1SampleComponent = vertex1SampleComponent;
						taskPathVertexN->vertex1SampleIndex = vertex1SampleIndex;
						break;
					}
				}
			}
		}
	}

	//if (get_global_id(0) == 0) {
	//	printf("============================================================\n");
	//	printf("== End loop\n");
	//	printf("============================================================\n");
	//}

#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	sampleResult->rayCount = tracedRaysCount;
#endif

	// Radiance clamping
	SR_RadianceClamp(sampleResult);

	taskStat->raysCount = tracedRaysCount;

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------
// MergePixelSamples
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void MergePixelSamples(
		const uint tileStartX,
		const uint tileStartY,
		const uint engineFilmWidth, const uint engineFilmHeight,
		__global SampleResult *taskResults,
		__global GPUTaskStats *taskStats,
		// Film parameters
		const uint filmWidth, const uint filmHeight
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
		, __global float *filmRadianceGroup0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
		, __global float *filmRadianceGroup1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
		, __global float *filmRadianceGroup2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
		, __global float *filmRadianceGroup3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
		, __global float *filmRadianceGroup4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
		, __global float *filmRadianceGroup5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
		, __global float *filmRadianceGroup6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
		, __global float *filmRadianceGroup7
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
		, __global float *filmAlpha
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
		, __global float *filmDepth
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		, __global float *filmPosition
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		, __global float *filmGeometryNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		, __global float *filmShadingNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		, __global uint *filmMaterialID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
		, __global float *filmDirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
		, __global float *filmDirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
		, __global float *filmEmission
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
		, __global float *filmIndirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
		, __global float *filmIndirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
		, __global float *filmIndirectSpecular
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
		, __global float *filmMaterialIDMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
		, __global float *filmDirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		, __global float *filmIndirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		, __global float *filmUV
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
		, __global float *filmRayCount
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
		, __global float *filmByMaterialID
#endif
		) {
	const size_t gid = get_global_id(0);

	uint sampleX, sampleY;
	sampleX = gid % PARAM_TILE_SIZE;
	sampleY = gid / PARAM_TILE_SIZE;

	if ((gid >= PARAM_TILE_SIZE * PARAM_TILE_SIZE) ||
			(tileStartX + sampleX >= engineFilmWidth) ||
			(tileStartY + sampleY >= engineFilmHeight))
		return;

	const uint index = gid * PARAM_AA_SAMPLES * PARAM_AA_SAMPLES;
	__global SampleResult *sampleResult = &taskResults[index];

	//--------------------------------------------------------------------------
	// Initialize Film radiance group pointer table
	//--------------------------------------------------------------------------

	__global float *filmRadianceGroup[PARAM_FILM_RADIANCE_GROUP_COUNT];
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	filmRadianceGroup[0] = filmRadianceGroup0;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	filmRadianceGroup[1] = filmRadianceGroup1;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	filmRadianceGroup[2] = filmRadianceGroup2;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	filmRadianceGroup[3] = filmRadianceGroup3;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	filmRadianceGroup[3] = filmRadianceGroup4;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	filmRadianceGroup[3] = filmRadianceGroup5;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	filmRadianceGroup[3] = filmRadianceGroup6;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	filmRadianceGroup[3] = filmRadianceGroup7;
#endif

	//--------------------------------------------------------------------------
	// Merge all samples and accumulate statistics
	//--------------------------------------------------------------------------

#if (PARAM_AA_SAMPLES == 1)
	Film_AddSample(sampleX, sampleY, &sampleResult[0], PARAM_AA_SAMPLES * PARAM_AA_SAMPLES
			FILM_PARAM);
#else
	__global GPUTaskStats *taskStat = &taskStats[index];

	SampleResult result = sampleResult[0];
	uint totalRaysCount = 0;
	for (uint i = 1; i < PARAM_AA_SAMPLES * PARAM_AA_SAMPLES; ++i) {
		SR_Accumulate(&sampleResult[i], &result);
		totalRaysCount += taskStat[i].raysCount;
	}
	SR_Normalize(&result, 1.f / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES));

	// I have to save result in __global space in order to be able
	// to use Film_AddSample(). OpenCL can be so stupid some time...
	sampleResult[0] = result;
	Film_AddSample(sampleX, sampleY, &sampleResult[0], PARAM_AA_SAMPLES * PARAM_AA_SAMPLES
			FILM_PARAM);

	taskStat[0].raysCount = totalRaysCount;
#endif
}
