#line 2 "material_funcs.cl"

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

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MATTE)

float3 MatteMaterial_Evaluate(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	const float3 kd = Texture_GetColorValue(&texs[material->matte.kdTexIndex], uv
			IMAGEMAPS_PARAM);
	return M_1_PI_F * kd;
}

float3 MatteMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	const float3 kd = Texture_GetColorValue(&texs[material->matte.kdTexIndex], uv
			IMAGEMAPS_PARAM);
	return M_1_PI_F * kd;
}

#endif

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MIRROR)

float3 MirrorMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	*event = SPECULAR | REFLECT;

	*sampledDir = (float3)(-fixedDir.x, -fixedDir.y, fixedDir.z);
	*pdfW = 1.f;

	*cosSampledDir = fabs((*sampledDir).z);
	const float3 kr = Texture_GetColorValue(&texs[material->mirror.krTexIndex], uv
			IMAGEMAPS_PARAM);
	// The cosSampledDir is used to compensate the other one used inside the integrator
	return kr / (*cosSampledDir);
}

#endif

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_GLASS)

float3 GlassMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);
	const float3 N = (float3)(0.f, 0.f, 1.f);

	const float3 rayDir = -fixedDir;
	const float3 reflDir = rayDir - (2.f * dot(N, rayDir)) * N;

	const float nc = Texture_GetGreyValue(&texs[material->glass.ousideIorTexIndex], uv
			IMAGEMAPS_PARAM);
	const float nt = Texture_GetGreyValue(&texs[material->glass.iorTexIndex], uv
			IMAGEMAPS_PARAM);
	const float nnt = into ? (nc / nt) : (nt / nc);
	const float nnt2 = nnt * nnt;
	const float ddn = dot(rayDir, shadeN);
	const float cos2t = 1.f - nnt2 * (1.f - ddn * ddn);

	// Total internal reflection
	if (cos2t < 0.f) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = 1.f;

		const float3 kr = Texture_GetColorValue(&texs[material->glass.krTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kr / (*cosSampledDir);
	}

	const float kk = (into ? 1.f : -1.f) * (ddn * nnt + sqrt(cos2t));
	const float3 nkk = kk * N;
	const float3 transDir = normalize(nnt * rayDir - nkk);

	const float c = 1.f - (into ? -ddn : dot(transDir, N));
	const float c2 = c * c;
	const float a = nt - nc;
	const float b = nt + nc;
	const float R0 = a * a / (b * b);
	const float Re = R0 + (1.f - R0) * c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f) {
		if (Re == 0.f)
			return BLACK;
		else {
			*event = SPECULAR | REFLECT;
			*sampledDir = reflDir;
			*cosSampledDir = fabs((*sampledDir).z);
			*pdfW = 1.f;

			const float3 kr = Texture_GetColorValue(&texs[material->glass.krTexIndex], uv
					IMAGEMAPS_PARAM);
			// The cosSampledDir is used to compensate the other one used inside the integrator
			return kr / (*cosSampledDir);
		}
	} else if (Re == 0.f) {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = 1.f;

		// The cosSampledDir is used to compensate the other one used inside the integrator
//		if (fromLight)
//			return Kt->GetColorValue(uv) * (nnt2 / (*cosSampledDir));
//		else
//			return Kt->GetColorValue(uv) / (*cosSampledDir);
		const float3 kt = Texture_GetColorValue(&texs[material->glass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		return kt / (*cosSampledDir);
	} else if (passThroughEvent < P) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = P / Re;

		const float3 kr = Texture_GetColorValue(&texs[material->glass.krTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kr / (*cosSampledDir);
	} else {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = (1.f - P) / Tr;

		// The cosSampledDir is used to compensate the other one used inside the integrator
//		if (fromLight)
//			return Kt->GetColorValue(uv) * (nnt2 / (*cosSampledDir));
//		else
//			return Kt->GetColorValue(uv) / (*cosSampledDir);
		const float3 kt = Texture_GetColorValue(&texs[material->glass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		return kt / (*cosSampledDir);
	}
}

#endif

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

float3 GlossyReflection(const float3 fixedDir, const float exponent,
		const float u0, const float u1) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);

	const float phi = 2.f * M_PI * u0;
	const float cosTheta = pow(1.f - u1, exponent);
	const float sinTheta = sqrt(fmax(0.f, 1.f - cosTheta * cosTheta));
	const float x = cos(phi) * sinTheta;
	const float y = sin(phi) * sinTheta;
	const float z = cosTheta;

	const float3 dir = -fixedDir;
	const float dp = dot(shadeN, dir);
	const float3 w = dir - (2.f * dp) * shadeN;

	const float3 u = normalize(cross(
			(fabs(shadeN.x) > .1f) ? ((float3)(0.f, 1.f, 0.f)) : ((float3)(1.f, 0.f, 0.f)),
			w));
	const float3 v = cross(w, u);

	return x * u + y * v + z * w;
}

#if defined (PARAM_ENABLE_MAT_METAL)

float3 MetalMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	const float e = 1.f / (Texture_GetGreyValue(&texs[material->metal.expTexIndex], uv
				IMAGEMAPS_PARAM) + 1.f);
	*sampledDir = GlossyReflection(fixedDir, e, u0, u1);

	if ((*sampledDir).z * fixedDir.z > 0.f) {
		*event = SPECULAR | REFLECT;
		*pdfW = 1.f;
		*cosSampledDir = fabs((*sampledDir).z);

		const float3 kt = Texture_GetColorValue(&texs[material->metal.krTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kt / (*cosSampledDir);
	} else
		return BLACK;
}

#endif

//------------------------------------------------------------------------------
// ArchGlass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_ARCHGLASS)

float3 ArchGlassMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);
	const float3 N = (float3)(0.f, 0.f, 1.f);

	const float3 rayDir = -fixedDir;
	const float3 reflDir = rayDir - (2.f * dot(N, rayDir)) * N;

	const float ddn = dot(rayDir, shadeN);
	const float cos2t = ddn * ddn;

	// Total internal reflection is not possible
	const float kk = (into ? 1.f : -1.f) * (ddn + sqrt(cos2t));
	const float3 nkk = kk * N;
	const float3 transDir = normalize(rayDir - nkk);

	const float c = 1.f - (into ? -ddn : dot(transDir, N));
	const float c2 = c * c;
	const float Re = c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f) {
		if (Re == 0.f)
			return BLACK;
		else {
			*event = SPECULAR | REFLECT;
			*sampledDir = reflDir;
			*cosSampledDir = fabs((*sampledDir).z);
			*pdfW = 1.f;

			const float3 kr = Texture_GetColorValue(&texs[material->archglass.krTexIndex], uv
					IMAGEMAPS_PARAM);
			// The cosSampledDir is used to compensate the other one used inside the integrator
			return kr / (*cosSampledDir);
		}
	} else if (Re == 0.f) {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = 1.f;

		const float3 kt = Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kt / (*cosSampledDir);
	} else if (passThroughEvent < P) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = P / Re;

		const float3 kr = Texture_GetColorValue(&texs[material->archglass.krTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kr / (*cosSampledDir);
	} else {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = (1.f - P) / Tr;

		const float3 kt = Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kt / (*cosSampledDir);
	}
}

float3 ArchGlassMaterial_GetPassThroughTransparency(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, const float passThroughEvent
		IMAGEMAPS_PARAM_DECL) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);
	const float3 N = (float3)(0.f, 0.f, 1.f);

	const float3 rayDir = -fixedDir;

	const float ddn = dot(rayDir, shadeN);
	const float cos2t = ddn * ddn;

	// Total internal reflection is not possible
	const float kk = (into ? 1.f : -1.f) * (ddn + sqrt(cos2t));
	const float3 nkk = kk * N;
	const float3 transDir = normalize(rayDir - nkk);

	const float c = 1.f - (into ? -ddn : dot(transDir, N));
	const float c2 = c * c;
	const float Re = c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f)
		return BLACK;
	else if (Re == 0.f) {
		const float3 kt = Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		return kt;
	} else if (passThroughEvent < P)
		return BLACK;
	else {
		const float3 kt = Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		return kt;
	}
}

#endif

//------------------------------------------------------------------------------
// NULL material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_NULL)

float3 NullMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	*sampledDir = -fixedDir;
	*cosSampledDir = 1.f;

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	return WHITE;
}

#endif

//------------------------------------------------------------------------------
// MatteTranslucent material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)

float3 MatteTranslucentMaterial_Evaluate(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float cosSampledDir = dot(lightDir, eyeDir);
	if (fabs(cosSampledDir) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	const float3 r = Texture_GetColorValue(&texs[material->matteTranslucent.krTexIndex], uv
			IMAGEMAPS_PARAM);
	const float3 t = Texture_GetColorValue(&texs[material->matteTranslucent.ktTexIndex], uv
			IMAGEMAPS_PARAM) * 
		// Energy conservation
		(1.f - r);

	if (directPdfW)
		*directPdfW = .5f * fabs(lightDir.z * M_1_PI_F);

	if (cosSampledDir > 0.f) {
		*event = DIFFUSE | REFLECT;
		return r * M_1_PI_F;
	} else {
		*event = DIFFUSE | TRANSMIT;
		return t * M_1_PI_F;
	}
}

float3 MatteTranslucentMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*sampledDir = CosineSampleHemisphereWithPdf(u0, u1, pdfW);
	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*pdfW *= .5f;

	const float3 r = Texture_GetColorValue(&texs[material->matteTranslucent.krTexIndex], uv
			IMAGEMAPS_PARAM);
	const float3 t = Texture_GetColorValue(&texs[material->matteTranslucent.ktTexIndex], uv
			IMAGEMAPS_PARAM) * 
		// Energy conservation
		(1.f - r);

	if (passThroughEvent < .5f) {
		*sampledDir *= (signbit(fixedDir.z) ? -1.f : 1.f);
		*event = DIFFUSE | REFLECT;
		return r * M_1_PI_F;
	} else {
		*sampledDir *= -(signbit(fixedDir.z) ? -1.f : 1.f);
		*event = DIFFUSE | TRANSMIT;
		return t * M_1_PI_F;
	}
}

#endif

//------------------------------------------------------------------------------
// Generic material functions
//
// They include the support for all material but Mix
// (because OpenCL doesn't support recursion)
//------------------------------------------------------------------------------

bool Material_IsDeltaNoMix(__global Material *material) {
	switch (material->type) {
		// Non Specular materials
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
#endif
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return false;
#endif
		// Specular materials
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
#endif
		default:
			return true;
	}
}

BSDFEvent Material_GetEventTypesNoMix(__global Material *mat) {
	switch (mat->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return DIFFUSE | REFLECT;
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return SPECULAR | REFLECT;
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return SPECULAR | REFLECT | TRANSMIT;
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
			return SPECULAR | REFLECT;
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return SPECULAR | REFLECT | TRANSMIT;
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return SPECULAR | TRANSMIT;
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return DIFFUSE | REFLECT | TRANSMIT;
#endif
		default:
			return NONE;
	}
}

float3 Material_SampleNoMix(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Sample(material, texs, uv, fixedDir, sampledDir,
					u0, u1,	pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_Sample(material, texs, uv, fixedDir, sampledDir,
					u0, u1, pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return GlassMaterial_Sample(material, texs, uv, fixedDir, sampledDir,
					u0, u1,	passThroughEvent, pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
			return MetalMaterial_Sample(material, texs, uv, fixedDir, sampledDir,
					u0, u1,	pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_Sample(material, texs, uv, fixedDir, sampledDir,
					u0, u1,	passThroughEvent, pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_Sample(material, texs, uv, fixedDir, sampledDir,
					u0, u1, pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_Sample(material, texs, uv, fixedDir, sampledDir,
					u0, u1,	passThroughEvent, pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
		default:
			return BLACK;
	}
}

float3 Material_EvaluateNoMix(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Evaluate(material, texs, uv, lightDir, eyeDir, event, directPdfW
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_Evaluate(material, texs, uv, lightDir, eyeDir, event, directPdfW
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
#endif
		default:
			return BLACK;
	}
}

float3 Material_GetEmittedRadianceNoMix(__global Material *material, __global Texture *texs, const float2 uv
		IMAGEMAPS_PARAM_DECL) {
	const uint emitTexIndex = material->emitTexIndex;
	if (emitTexIndex == NULL_INDEX)
		return BLACK;

	return Texture_GetColorValue(&texs[emitTexIndex], uv
			IMAGEMAPS_PARAM);
}

//------------------------------------------------------------------------------
// Mix material
//
// This requires a quite complex implementation because OpenCL doesn't support
// recursion.
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MIX)

#define MIX_STACK_SIZE 16

BSDFEvent MixMaterial_IsDelta(__global Material *material
		MATERIALS_PARAM_DECL) {
	__global Material *materialStack[MIX_STACK_SIZE];
	materialStack[0] = material;
	int stackIndex = 0;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex--];

		// Check if it is a Mix material too
		if (m->type == MIX) {
			// Add both material to the stack
			materialStack[++stackIndex] = &mats[m->mix.matAIndex];
			materialStack[++stackIndex] = &mats[m->mix.matBIndex];
		} else {
			// Normal GetEventTypes() evaluation
			if (!Material_IsDeltaNoMix(m))
				return false;
		}
	}

	return true;
}

BSDFEvent MixMaterial_GetEventTypes(__global Material *material
		MATERIALS_PARAM_DECL) {
	BSDFEvent event = NONE;
	__global Material *materialStack[MIX_STACK_SIZE];
	materialStack[0] = material;
	int stackIndex = 0;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex--];

		// Check if it is a Mix material too
		if (m->type == MIX) {
			// Add both material to the stack
			materialStack[++stackIndex] = &mats[m->mix.matAIndex];
			materialStack[++stackIndex] = &mats[m->mix.matBIndex];
		} else {
			// Normal GetEventTypes() evaluation
			event |= Material_GetEventTypesNoMix(m);
		}
	}

	return event;
}

float3 MixMaterial_Evaluate(__global Material *material,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL
		IMAGEMAPS_PARAM_DECL) {
	__global Material *materialStack[MIX_STACK_SIZE];
	float totalWeightStack[MIX_STACK_SIZE];

	// Push the root Mix material
	materialStack[0] = material;
	totalWeightStack[0] = 1.f;
	int stackIndex = 0;

	// Setup the results
	float3 result = BLACK;
	*event = NONE;
	if (directPdfW)
		*directPdfW = 0.f;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex];
		float totalWeight = totalWeightStack[stackIndex--];

		// Check if it is a Mix material too
		if (m->type == MIX) {
			// Add both material to the stack
			const float factor = Texture_GetGreyValue(&texs[m->mix.mixFactorTexIndex], uv
					IMAGEMAPS_PARAM);
			const float weight2 = clamp(factor, 0.f, 1.f);
			const float weight1 = 1.f - weight2;

			materialStack[++stackIndex] = &mats[m->mix.matAIndex];
			totalWeightStack[stackIndex] = totalWeight * weight1;

			materialStack[++stackIndex] = &mats[m->mix.matBIndex];			
			totalWeightStack[stackIndex] = totalWeight * weight2;
		} else {
			// Normal GetEventTypes() evaluation
			if (totalWeight > 0.f) {
				BSDFEvent eventMat;
				float directPdfWMat;
				const float3 resultMat = Material_EvaluateNoMix(m, texs, uv, lightDir, eyeDir, &eventMat, &directPdfWMat
						IMAGEMAPS_PARAM);
				if (any(isnotequal(resultMat, BLACK))) {
					result += totalWeight * resultMat;

					if (directPdfW)
						*directPdfW += totalWeight * directPdfWMat;
				}
				
				*event |= eventMat;
			}
		}
	}

	return result;
}

float3 MixMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, const float passEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL
		IMAGEMAPS_PARAM_DECL) {
	__global Material *evaluationMatList[MIX_STACK_SIZE];
	float parentWeightList[MIX_STACK_SIZE];
	int evaluationListSize = 0;

	// Setup the results
	float3 result = BLACK;
	*pdfW = 0.f;

	// Look for a no Mix material to sample
	__global Material *currentMixMat = material;
	float passThroughEvent = passEvent;
	float parentWeight = 1.f;
	for (;;) {
		const float factor = Texture_GetGreyValue(&texs[currentMixMat->mix.mixFactorTexIndex], uv
			IMAGEMAPS_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		const bool sampleMatA = (passThroughEvent < weight1);

		const float weightFirst = sampleMatA ? weight1 : weight2;
		const float weightSecond = sampleMatA ? weight2 : weight1;

		const float passThroughEventFirst = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;

		const uint matIndexFirst = sampleMatA ? currentMixMat->mix.matAIndex : currentMixMat->mix.matBIndex;
		const uint matIndexSecond = sampleMatA ? currentMixMat->mix.matBIndex : currentMixMat->mix.matAIndex;

		// Sample the first material, evaluate the second
		__global Material *matFirst = &mats[matIndexFirst];
		__global Material *matSecond = &mats[matIndexSecond];

		//----------------------------------------------------------------------
		// Add the second material to the evaluation list
		//----------------------------------------------------------------------

		if (weightSecond > 0.f) {
			evaluationMatList[evaluationListSize] = matSecond;
			parentWeightList[evaluationListSize++] = parentWeight * weightSecond;
		}

		//----------------------------------------------------------------------
		// Sample the first material
		//----------------------------------------------------------------------

		// Check if it is a Mix material too
		if (matFirst->type == MIX) {
			// Make the first material the current
			currentMixMat = matFirst;
			passThroughEvent = passThroughEventFirst;
			parentWeight *= weightFirst;
		} else {
			// Sample the first material
			float pdfWMatFirst;
			const float3 sampleResult = Material_SampleNoMix(matFirst, texs, uv,
					fixedDir, sampledDir,
					u0, u1, passThroughEventFirst,
					&pdfWMatFirst, cosSampledDir, event
					IMAGEMAPS_PARAM);

			if (all(isequal(sampleResult, BLACK)))
				return BLACK;

			const float weight = parentWeight * weightFirst;
			*pdfW += weight * pdfWMatFirst;
			result += weight * sampleResult;

			// I can stop now
			break;
		}
	}

	while (evaluationListSize > 0) {
		// Extract the material to evaluate
		__global Material *evalMat = evaluationMatList[--evaluationListSize];
		const float evalWeight = parentWeightList[evaluationListSize];

		// Evaluate the material

		// Check if it is a Mix material too
		BSDFEvent eventMat;
		float pdfWMat;
		float3 eval;
		if (evalMat->type == MIX) {
			eval = MixMaterial_Evaluate(evalMat, uv, *sampledDir, fixedDir,
					&eventMat, &pdfWMat
					MATERIALS_PARAM
					IMAGEMAPS_PARAM);
		} else {
			eval = Material_EvaluateNoMix(evalMat, texs, uv, *sampledDir, fixedDir,
					&eventMat, &pdfWMat
					IMAGEMAPS_PARAM);
		}
		if (any(isnotequal(eval, BLACK))) {
			result += evalWeight * eval;
			*pdfW += evalWeight * pdfWMat;
		}
	}

	return result;
}

float3 MixMaterial_GetEmittedRadiance(__global Material *material, const float2 uv
		MATERIALS_PARAM_DECL
		IMAGEMAPS_PARAM_DECL) {
	__global Material *materialStack[MIX_STACK_SIZE];
	float totalWeightStack[MIX_STACK_SIZE];

	// Push the root Mix material
	materialStack[0] = material;
	totalWeightStack[0] = 1.f;
	int stackIndex = 0;

	// Setup the results
	float3 result = BLACK;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex];
		float totalWeight = totalWeightStack[stackIndex--];

		if (m->type == MIX) {
			const float factor = Texture_GetGreyValue(&texs[m->mix.mixFactorTexIndex], uv
					IMAGEMAPS_PARAM);
			const float weight2 = clamp(factor, 0.f, 1.f);
			const float weight1 = 1.f - weight2;

			if (weight1 > 0.f) {
				materialStack[++stackIndex] = &mats[m->mix.matAIndex];
				totalWeightStack[stackIndex] = totalWeight * weight1;
			}

			if (weight2 > 0.f) {
				materialStack[++stackIndex] = &mats[m->mix.matBIndex];
				totalWeightStack[stackIndex] = totalWeight * weight2;
			}
		} else {
			const float3 emitRad = Material_GetEmittedRadianceNoMix(m, texs, uv
				IMAGEMAPS_PARAM);
			if (any(isnotequal(emitRad, BLACK)))
				result += totalWeight * emitRad;
		}
	}
	
	return result;
}

#endif

//------------------------------------------------------------------------------
// Generic material functions with Mix support
//------------------------------------------------------------------------------

BSDFEvent Material_GetEventTypes(__global Material *material
		MATERIALS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_GetEventTypes(material
				MATERIALS_PARAM);
	else
#endif
		return Material_GetEventTypesNoMix(material);
}

bool Material_IsDelta(__global Material *material
		MATERIALS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_IsDelta(material
				MATERIALS_PARAM);
	else
#endif
		return Material_IsDeltaNoMix(material);
}

float3 Material_Evaluate(__global Material *material,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL
		IMAGEMAPS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_Evaluate(material, uv, lightDir, eyeDir,
				event, directPdfW
				MATERIALS_PARAM
				IMAGEMAPS_PARAM);
	else
#endif
		return Material_EvaluateNoMix(material, texs, uv, lightDir, eyeDir,
				event, directPdfW
				IMAGEMAPS_PARAM);
}

float3 Material_Sample(__global Material *material,	const float2 uv,
		const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL
		IMAGEMAPS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_Sample(material, uv,
				fixedDir, sampledDir,
				u0, u1,
				passThroughEvent,
				pdfW, cosSampledDir, event
				MATERIALS_PARAM
				IMAGEMAPS_PARAM);
	else
#endif
		return Material_SampleNoMix(material, texs, uv,
				fixedDir, sampledDir,
				u0, u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
				passThroughEvent,
#endif
				pdfW, cosSampledDir, event
				IMAGEMAPS_PARAM);
}

float3 Material_GetEmittedRadiance(__global Material *material, const float2 uv
		MATERIALS_PARAM_DECL
		IMAGEMAPS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_GetEmittedRadiance(material, uv
				MATERIALS_PARAM
				IMAGEMAPS_PARAM);
	else
#endif
		return Material_GetEmittedRadianceNoMix(material, texs, uv
				IMAGEMAPS_PARAM);
}

#if defined(PARAM_HAS_PASSTHROUGHT)
float3 Material_GetPassThroughTransparency(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, const float passThroughEvent
		IMAGEMAPS_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_GetPassThroughTransparency(material, texs,
					uv, fixedDir, passThroughEvent
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return WHITE;
#endif
		default:
			return BLACK;
	}
}
#endif
