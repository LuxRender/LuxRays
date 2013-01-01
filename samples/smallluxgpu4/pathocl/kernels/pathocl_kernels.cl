#line 2 "patchocl_kernels.cl"

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

// List of symbols defined at compile time:
//  PARAM_TASK_COUNT
//  PARAM_IMAGE_WIDTH
//  PARAM_IMAGE_HEIGHT
//  PARAM_RAY_EPSILON
//  PARAM_MAX_PATH_DEPTH
//  PARAM_MAX_RR_DEPTH
//  PARAM_MAX_RR_CAP
//  PARAM_HAS_TEXTUREMAPS
//  PARAM_HAS_ALPHA_TEXTUREMAPS
//  PARAM_USE_PIXEL_ATOMICS
//  PARAM_HAS_BUMPMAPS
//  PARAM_ACCEL_BVH or PARAM_ACCEL_QBVH or PARAM_ACCEL_MQBVH

// To enable single material support (work around for ATI compiler problems)
//  PARAM_ENABLE_MAT_MATTE
//  PARAM_ENABLE_MAT_AREALIGHT
//  PARAM_ENABLE_MAT_MIRROR
//  PARAM_ENABLE_MAT_GLASS
//  PARAM_ENABLE_MAT_MATTEMIRROR
//  PARAM_ENABLE_MAT_METAL
//  PARAM_ENABLE_MAT_MATTEMETAL
//  PARAM_ENABLE_MAT_ALLOY
//  PARAM_ENABLE_MAT_ARCHGLASS

// (optional)
//  PARAM_DIRECT_LIGHT_SAMPLING
//  PARAM_DL_LIGHT_COUNT

// (optional)
//  PARAM_CAMERA_HAS_DOF

// (optional)
//  PARAM_HAS_INFINITELIGHT

// (optional, requires PARAM_DIRECT_LIGHT_SAMPLING)
//  PARAM_HAS_SUNLIGHT

// (optional)
//  PARAM_HAS_SKYLIGHT

// (optional)
//  PARAM_IMAGE_FILTER_TYPE (0 = No filter, 1 = Box, 2 = Gaussian, 3 = Mitchell)
//  PARAM_IMAGE_FILTER_WIDTH_X
//  PARAM_IMAGE_FILTER_WIDTH_Y
// (Box filter)
// (Gaussian filter)
//  PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA
// (Mitchell filter)
//  PARAM_IMAGE_FILTER_MITCHELL_B
//  PARAM_IMAGE_FILTER_MITCHELL_C

// (optional)
//  PARAM_SAMPLER_TYPE (0 = Inlined Random, 1 = Metropolis)
// (Metropolis)
//  PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE
//  PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT
//  PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE

// TODO: IDX_BSDF_Z used only if needed

// (optional)
//  PARAM_ENABLE_ALPHA_CHANNEL

//------------------------------------------------------------------------------
// Init Kernel
//------------------------------------------------------------------------------

__kernel void Init(
		uint seedBase,
		__global GPUTask *tasks,
		__global float *samplesData,
		__global GPUTaskStats *taskStats,
		__global Ray *rays,
		__global Camera *camera
		) {
	const size_t gid = get_global_id(0);

	//if (gid == 0)
	//	printf("GPUTask: %d\n", sizeof(GPUTask));

	// Initialize the task
	__global GPUTask *task = &tasks[gid];
	__global Sample *sample = &task->sample;
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);

	// Initialize random number generator
	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Initialize the sample
	Sampler_Init(&seed, &task->sample, sampleData);

	// Initialize the path
	GenerateCameraPath(task, sampleData,
#if (PARAM_SAMPLER_TYPE == 0)
		&seed,
#endif
		camera, &rays[gid]);

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;

	__global GPUTaskStats *taskStat = &taskStats[gid];
	taskStat->sampleCount = 0;
}

//------------------------------------------------------------------------------
// InitFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel void InitFrameBuffer(
		__global Pixel *frameBuffer
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		, __global AlphaPixel *alphaFrameBuffer
#endif
		) {
	const size_t gid = get_global_id(0);
	if (gid >= (PARAM_IMAGE_WIDTH + 2) * (PARAM_IMAGE_HEIGHT + 2))
		return;

	__global Pixel *p = &frameBuffer[gid];
	p->c.r = 0.f;
	p->c.g = 0.f;
	p->c.b = 0.f;
	p->count = 0.f;

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	__global AlphaPixel *ap = &alphaFrameBuffer[gid];
	ap->alpha = 0.f;
#endif
}

//------------------------------------------------------------------------------
// AdvancePaths Kernel
//------------------------------------------------------------------------------

//#if defined(PARAM_HAS_TEXTUREMAPS)
//
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_0)
//__global Spectrum *GetRGBAddress(const uint page, const uint offset
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_0)
//                , __global Spectrum *texMapRGBBuff0
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_1)
//                , __global Spectrum *texMapRGBBuff1
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_2)
//                , __global Spectrum *texMapRGBBuff2
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_3)
//                , __global Spectrum *texMapRGBBuff3
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_4)
//                , __global Spectrum *texMapRGBBuff4
//#endif
//    ) {
//    switch (page) {
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_1)
//        case 1:
//            return &texMapRGBBuff1[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_2)
//        case 2:
//            return &texMapRGBBuff2[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_3)
//        case 3:
//            return &texMapRGBBuff3[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_4)
//        case 4:
//            return &texMapRGBBuff4[offset];
//#endif
//        default:
//        case 0:
//            return &texMapRGBBuff0[offset];
//    }
//}
//#endif
//
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_0)
//__global float *GetAlphaAddress(const uint page, const uint offset
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_0)
//                , __global float *texMapAlphaBuff0
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_1)
//                , __global float *texMapAlphaBuff1
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_2)
//                , __global float *texMapAlphaBuff2
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_3)
//                , __global float *texMapAlphaBuff3
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_4)
//                , __global float *texMapAlphaBuff4
//#endif
//    ) {
//    switch (page) {
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_1)
//        case 1:
//            return &texMapAlphaBuff1[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_2)
//        case 2:
//            return &texMapAlphaBuff2[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_3)
//        case 3:
//            return &texMapAlphaBuff3[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_4)
//        case 4:
//            return &texMapAlphaBuff4[offset];
//#endif
//        default:
//        case 0:
//            return &texMapAlphaBuff0[offset];
//    }
//}
//#endif
//
//#endif

__kernel void AdvancePaths(
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global float *samplesData,
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Pixel *frameBuffer,
//		__global Material *mats,
//		__global uint *meshMats,
		__global uint *meshIDs,
#if defined(PARAM_ACCEL_MQBVH)
		__global uint *meshFirstTriangleOffset,
		__global Mesh *meshDescs,
#endif
		__global Vector *vertNormals,
		__global Point *vertices,
		__global Triangle *triangles,
		__global Camera *camera
		) {
	const size_t gid = get_global_id(0);

	__global GPUTask *task = &tasks[gid];
	__global Sample *sample = &task->sample;
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);

	// Read the seed
	Seed seedValue;
	seedValue.s1 = task->seed.s1;
	seedValue.s2 = task->seed.s2;
	seedValue.s3 = task->seed.s3;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	// read the path state
	PathState pathState = task->pathStateBase.state;

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
	const uint currentTriangleIndex = rayHit->index;

	const float3 skyCol = (float3)(.5f, .75f, 1.f);

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: RT_NEXT_VERTEX
	// To: SPLAT_SAMPLE or GENERATE_DL_RAY
	//--------------------------------------------------------------------------

	if (pathState == RT_NEXT_VERTEX) {
		if (currentTriangleIndex != 0xffffffffu) {
			// Something was hit

			__global BSDF *bsdf = &task->pathStateBase.bsdf;
			BSDF_Init(bsdf,
#if defined(PARAM_ACCEL_MQBVH)
					meshFirstTriangleOffset,
					meshDescs,
#endif
					//mats,
					//meshMats,
					meshIDs, vertNormals, vertices, triangles, ray, rayHit,
					Sampler_GetSample(IDX_DOF_X));

			// Sample next path vertex
			pathState = GENERATE_NEXT_VERTEX_RAY;
		} else {
			sample->radiance.r += skyCol.s0;
			sample->radiance.g += skyCol.s1;
			sample->radiance.b += skyCol.s2;
			pathState = SPLAT_SAMPLE;
		}
	}

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: GENERATE_NEXT_VERTEX_RAY
	// To: SPLAT_SAMPLE or RT_NEXT_VERTEX
	//--------------------------------------------------------------------------

	if (pathState == GENERATE_NEXT_VERTEX_RAY) {
		uint depth = task->pathStateBase.depth;

		if (depth < PARAM_MAX_PATH_DEPTH) {
			// Sample the BSDF
			__global BSDF *bsdf = &task->pathStateBase.bsdf;
			float3 sampledDir;
			float lastPdfW;
			float cosSampledDir;
			BSDFEvent event;

			const float3 bsdfSample = BSDF_Sample(bsdf, &sampledDir,
					Sampler_GetSample(IDX_BSDF_X), Sampler_GetSample(IDX_BSDF_Y),
					&lastPdfW, &cosSampledDir, &event);
			const bool lastSpecular = ((event & SPECULAR) != 0);

			// Russian Roulette
			const float rrProb = fmax(Spectrum_Filter(bsdfSample), PARAM_RR_CAP);
			const bool rrContinuePath = (depth < PARAM_RR_DEPTH) ||
				lastSpecular || (Sampler_GetSample(IDX_RR) < rrProb);

			const bool continuePath = !all(isequal(bsdfSample, BLACK)) && rrContinuePath;
			if (continuePath) {
				//lastPdfW *= rrProb; // Russian Roulette

				float3 throughput = vload3(0, &task->pathStateBase.throughput.r);
				throughput *= bsdfSample * (cosSampledDir / lastPdfW);
				vstore3(throughput, 0, &task->pathStateBase.throughput.r);

				//TODO: Ray_Init(bsdf.hitPoint, sampledDir);
				ray->o = bsdf->hitPoint;
				vstore3(sampledDir, 0, &ray->d.x);
				ray->mint = PARAM_RAY_EPSILON;
				ray->maxt = INFINITY;

				task->pathStateBase.depth = depth + 1;
				pathState = RT_NEXT_VERTEX;
			} else
				pathState = SPLAT_SAMPLE;
		} else
			pathState = SPLAT_SAMPLE;
	}

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: SPLAT_SAMPLE
	// To: RT_NEXT_VERTEX
	//--------------------------------------------------------------------------

	if (pathState == SPLAT_SAMPLE) {
		Sampler_NextSample(task, sample, sampleData, seed, frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				alphaFrameBuffer,
#endif
				camera, ray);
		taskStats[gid].sampleCount += 1;

		pathState = RT_NEXT_VERTEX;
	}

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed.s1 = seed->s1;
	task->seed.s2 = seed->s2;
	task->seed.s3 = seed->s3;

	// Save the state
	task->pathStateBase.state = pathState;
}
