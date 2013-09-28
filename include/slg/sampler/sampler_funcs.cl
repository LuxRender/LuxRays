#line 2 "sampler_funcs.cl"

/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

void SampleResult_Init(__global SampleResult *sampleResult) {
	// Initialize only Spectrum fields

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[0].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[1].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[2].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[3].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[4].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[5].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[6].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[7].r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	VSTORE3F(BLACK, &sampleResult->directDiffuse.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	VSTORE3F(BLACK, &sampleResult->directGlossy.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	VSTORE3F(BLACK, &sampleResult->emission.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	VSTORE3F(BLACK, &sampleResult->indirectDiffuse.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	VSTORE3F(BLACK, &sampleResult->indirectGlossy.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	VSTORE3F(BLACK, &sampleResult->indirectSpecular.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	sampleResult->directShadowMask = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	sampleResult->indirectShadowMask = 1.f;
#endif
}

//------------------------------------------------------------------------------
// Random Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 0)

#define Sampler_GetSamplePath(index) (Rnd_FloatValue(seed))
#define Sampler_GetSamplePathVertex(depth, index) (Rnd_FloatValue(seed))

__global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * TOTAL_U_SIZE];
}

__global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {
	return sampleData;
}

__global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,
		__global float *sampleDataPathBase, const uint depth) {
	return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE];
}

void Sampler_Init(Seed *seed, __global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight) {
	const float u0 = Rnd_FloatValue(seed);
	const float u1 = Rnd_FloatValue(seed);

	// TODO: remove sampleData[]
	sampleData[IDX_SCREEN_X] = u0;
	sampleData[IDX_SCREEN_Y] = u1;

	SampleResult_Init(&sample->result);
	sample->result.filmX = min(u0 * filmWidth, (float)(filmWidth - 1));
	sample->result.filmY = min(u1 * filmHeight, (float)(filmHeight - 1));
}

void Sampler_NextSample(
		Seed *seed,
		__global Sample *sample,
		__global float *sampleData
		FILM_PARAM_DECL
		) {
	Film_SplatSample(&sample->result, 1.f
			FILM_PARAM);

	// Move to the next assigned pixel
	const float u0 = Rnd_FloatValue(seed);
	const float u1 = Rnd_FloatValue(seed);

	// TODO: remove sampleData[]
	sampleData[IDX_SCREEN_X] = u0;
	sampleData[IDX_SCREEN_Y] = u1;

	SampleResult_Init(&sample->result);
	sample->result.filmX = min(u0 * filmWidth, (float)(filmWidth - 1));
	sample->result.filmY = min(u1 * filmHeight, (float)(filmHeight - 1));
}

#endif

//------------------------------------------------------------------------------
// Metropolis Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 1)

#define Sampler_GetSamplePath(index) (sampleDataPathBase[index])
#define Sampler_GetSamplePathVertex(depth, index) (sampleDataPathVertexBase[index])

__global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * (2 * TOTAL_U_SIZE)];
}

__global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {
	return &sampleData[sample->proposed * TOTAL_U_SIZE];
}

__global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,
		__global float *sampleDataPathBase, const uint depth) {
	return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE];
}

void LargeStep(Seed *seed, __global float *proposedU) {
	for (int i = 0; i < TOTAL_U_SIZE; ++i)
		proposedU[i] = Rnd_FloatValue(seed);
}

float Mutate(Seed *seed, const float x) {
	const float s1 = 1.f / 512.f;
	const float s2 = 1.f / 16.f;

	const float randomValue = Rnd_FloatValue(seed);

	const float dx = s1 / (s1 / s2 + fabs(2.f * randomValue - 1.f)) -
		s1 / (s1 / s2 + 1.f);

	float mutatedX = x;
	if (randomValue < 0.5f) {
		mutatedX += dx;
		mutatedX = (mutatedX < 1.f) ? mutatedX : (mutatedX - 1.f);
	} else {
		mutatedX -= dx;
		mutatedX = (mutatedX < 0.f) ? (mutatedX + 1.f) : mutatedX;
	}

	return mutatedX;
}

float MutateScaled(Seed *seed, const float x, const float range) {
	const float s1 = 32.f;

	const float randomValue = Rnd_FloatValue(seed);

	const float dx = range / (s1 / (1.f + s1) + (s1 * s1) / (1.f + s1) *
		fabs(2.f * randomValue - 1.f)) - range / s1;

	float mutatedX = x;
	if (randomValue < 0.5f) {
		mutatedX += dx;
		mutatedX = (mutatedX < 1.f) ? mutatedX : (mutatedX - 1.f);
	} else {
		mutatedX -= dx;
		mutatedX = (mutatedX < 0.f) ? (mutatedX + 1.f) : mutatedX;
	}

	return mutatedX;
}

void SmallStep(Seed *seed, __global float *currentU, __global float *proposedU) {
	proposedU[IDX_SCREEN_X] = MutateScaled(seed, currentU[IDX_SCREEN_X],
			PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE);
	proposedU[IDX_SCREEN_Y] = MutateScaled(seed, currentU[IDX_SCREEN_Y],
			PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE);

	for (int i = IDX_SCREEN_Y + 1; i < TOTAL_U_SIZE; ++i)
		proposedU[i] = Mutate(seed, currentU[i]);
}

void Sampler_Init(Seed *seed, __global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight) {
	sample->totalI = 0.f;
	sample->largeMutationCount = 1.f;

	sample->current = NULL_INDEX;
	sample->proposed = 1;

	sample->smallMutationCount = 0;
	sample->consecutiveRejects = 0;

	sample->weight = 0.f;

	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	LargeStep(seed, sampleDataPathBase);

	SampleResult_Init(&sample->result);
	sample->result.filmX = min(sampleDataPathBase[IDX_SCREEN_X] * filmWidth, (float)(filmWidth - 1));
	sample->result.filmY = min(sampleDataPathBase[IDX_SCREEN_Y] * filmHeight, (float)(filmHeight - 1));
}

void Sampler_NextSample(
		Seed *seed,
		__global Sample *sample,
		__global float *sampleData
		FILM_PARAM_DECL
		) {
	//--------------------------------------------------------------------------
	// Accept/Reject the sample
	//--------------------------------------------------------------------------

	uint current = sample->current;
	uint proposed = sample->proposed;

	if (current == NULL_INDEX) {
		// It is the very first sample, I have still to initialize the current
		// sample

		Film_SplatSample(&sample->result, 1.f
					FILM_PARAM);

		sample->currentResult = sample->result;
		sample->totalI = SampleResult_Radiance_Y(&sample->result);

		current = proposed;
		proposed ^= 1;
	} else {
		const float currentI = SampleResult_Radiance_Y(&sample->currentResult);

		float proposedI = SampleResult_Radiance_Y(&sample->result);
		proposedI = (isnan(proposedI) || isinf(proposedI)) ? 0.f : proposedI;

		float totalI = sample->totalI;
		uint largeMutationCount = sample->largeMutationCount;
		uint smallMutationCount = sample->smallMutationCount;
		if (smallMutationCount == 0) {
			// It is a large mutation
			totalI += proposedI;
			largeMutationCount += 1;

			sample->totalI = totalI;
			sample->largeMutationCount = largeMutationCount;
		}

		const float meanI = (totalI > 0.f) ? (totalI / largeMutationCount) : 1.f;

		// Calculate accept probability from old and new image sample
		uint consecutiveRejects = sample->consecutiveRejects;

		float accProb;
		if ((currentI > 0.f) && (consecutiveRejects < PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT))
			accProb = min(1.f, proposedI / currentI);
		else
			accProb = 1.f;

		const float newWeight = accProb + ((smallMutationCount == 0) ? 1.f : 0.f);
		float weight = sample->weight;
		weight += 1.f - accProb;

		const float rndVal = Rnd_FloatValue(seed);

		/*if (get_global_id(0) == 0)
			printf(\"[%d] Current: (%f, %f, %f) [%f] Proposed: (%f, %f, %f) [%f] accProb: %f <%f>\\n\",
					consecutiveRejects,
					currentL.r, currentL.g, currentL.b, weight,
					proposedL.r, proposedL.g, proposedL.b, newWeight,
					accProb, rndVal);*/

		__global SampleResult *contrib;
		float norm;

		if ((accProb == 1.f) || (rndVal < accProb)) {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tACCEPTED !\\n\");*/

			// Add accumulated contribution of previous reference sample
			norm = weight / (currentI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = &sample->currentResult;

			current ^= 1;
			proposed ^= 1;
			consecutiveRejects = 0;

			weight = newWeight;
		} else {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tREJECTED !\\n\");*/

			// Add contribution of new sample before rejecting it
			norm = newWeight / (proposedI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = &sample->result;

			++consecutiveRejects;
		}

		if (norm > 0.f) {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tContrib: (%f, %f, %f) [%f] consecutiveRejects: %d\\n\",
						contrib.r, contrib.g, contrib.b, norm, consecutiveRejects);*/

			Film_SplatSample(contrib, norm
					FILM_PARAM);
		}

		// Check if it is an accepted mutation
		if (consecutiveRejects == 0) {
			// I can now (after Film_SplatSample()) overwrite sample->currentResult and sample->result
			sample->currentResult = sample->result;
		}

		sample->weight = weight;
		sample->consecutiveRejects = consecutiveRejects;
	}

	sample->current = current;
	sample->proposed = proposed;

	//--------------------------------------------------------------------------
	// Mutate the sample
	//--------------------------------------------------------------------------

	__global float *proposedU = &sampleData[proposed * TOTAL_U_SIZE];
	if (Rnd_FloatValue(seed) < PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE) {
		LargeStep(seed, proposedU);
		sample->smallMutationCount = 0;
	} else {
		__global float *currentU = &sampleData[current * TOTAL_U_SIZE];

		SmallStep(seed, currentU, proposedU);
		sample->smallMutationCount += 1;
	}

	SampleResult_Init(&sample->result);
	sample->result.filmX = min(proposedU[IDX_SCREEN_X] * filmWidth, (float)(filmWidth - 1));
	sample->result.filmY = min(proposedU[IDX_SCREEN_Y] * filmHeight, (float)(filmHeight - 1));
}

#endif

//------------------------------------------------------------------------------
// Sobol Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 2)

uint SobolSampler_SobolDimension(const uint index, const uint dimension) {
	const uint offset = dimension * SOBOL_BITS;
	uint result = 0;
	uint i = index;

	for (uint j = 0; i; i >>= 1, j++) {
		if (i & 1)
			result ^= SOBOL_DIRECTIONS[offset + j];
	}

	return result;
}

float SobolSampler_GetSample(__global Sample *sample, const uint index) {
	const uint pass = sample->pass;

	const uint result = SobolSampler_SobolDimension(pass, index);
	const float r = result * (1.f / 0xffffffffu);

	// Cranley-Patterson rotation to reduce visible regular patterns
	const float shift = (index & 1) ? sample->rng0 : sample->rng1;

	return r + shift - floor(r + shift);
}

#define Sampler_GetSamplePath(index) (SobolSampler_GetSample(sample, index))
#define Sampler_GetSamplePathVertex(depth, index) ((depth > PARAM_SAMPLER_SOBOL_MAXDEPTH) ? \
	Rnd_FloatValue(seed) : \
	SobolSampler_GetSample(sample, IDX_BSDF_OFFSET + (depth - 1) * VERTEX_SAMPLE_SIZE + index))

__global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * TOTAL_U_SIZE];
}

__global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {
	return sampleData;
}

__global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,
		__global float *sampleDataPathBase, const uint depth) {
	return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE];
}

void Sampler_Init(Seed *seed, __global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight) {
	sample->rng0 = Rnd_FloatValue(seed);
	sample->rng1 = Rnd_FloatValue(seed);
	sample->pass = PARAM_SAMPLER_SOBOL_STARTOFFSET;

	const uint pixelIndex = get_global_id(0) + (PARAM_TASK_COUNT * PARAM_DEVICE_INDEX / PARAM_DEVICE_COUNT);
	sample->pixelIndex = pixelIndex;
	uint x, y;
	PixelIndex2XY(filmWidth, pixelIndex, &x, &y);

	const float u0 = (x + Sampler_GetSamplePath(IDX_SCREEN_X)) * (1.f / filmWidth);
	const float u1 = (y + Sampler_GetSamplePath(IDX_SCREEN_Y)) * (1.f / filmHeight);

	sampleData[IDX_SCREEN_X] = u0;
	sampleData[IDX_SCREEN_Y] = u1;

	SampleResult_Init(&sample->result);
	sample->result.filmX = min(u0 * filmWidth, (float)(filmWidth - 1));
	sample->result.filmY = min(u1 * filmHeight, (float)(filmHeight - 1));
}

void Sampler_NextSample(
		Seed *seed,
		__global Sample *sample,
		__global float *sampleData
		FILM_PARAM_DECL
		) {
	Film_SplatSample(&sample->result, 1.f
			FILM_PARAM);

	// Move to the next assigned pixel
	uint nextPixelIndex = sample->pixelIndex + PARAM_TASK_COUNT;
	if (nextPixelIndex > filmWidth * filmHeight) {
		nextPixelIndex = get_global_id(0) + (PARAM_TASK_COUNT * PARAM_DEVICE_INDEX / PARAM_DEVICE_COUNT);
		sample->pass += 1;
	}
	sample->pixelIndex = nextPixelIndex;
	uint x, y;
	PixelIndex2XY(filmWidth, nextPixelIndex, &x, &y);

	const float u0 = (x + Sampler_GetSamplePath(IDX_SCREEN_X)) * (1.f / filmWidth);
	const float u1 = (y + Sampler_GetSamplePath(IDX_SCREEN_Y)) * (1.f / filmHeight);

	sampleData[IDX_SCREEN_X] = u0;
	sampleData[IDX_SCREEN_Y] = u1;

	SampleResult_Init(&sample->result);
	sample->result.filmX = min(u0 * filmWidth, (float)(filmWidth - 1));
	sample->result.filmY = min(u1 * filmHeight, (float)(filmHeight - 1));
}

#endif

