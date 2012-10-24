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

#include "luxrays/core/geometry/frame.h"
#include "luxrays/utils/sdl/light.h"
#include "luxrays/utils/sdl/spd.h"
#include "luxrays/utils/sdl/data/sun_spect.h"
#include "luxrays/utils/sdl/scene.h"

using namespace luxrays;
using namespace luxrays::sdl;

//------------------------------------------------------------------------------
// SkyLight
//------------------------------------------------------------------------------

float PerezBase(const float lam[6], float theta, float gamma) {
	return (1.f + lam[1] * expf(lam[2] / cosf(theta))) *
		(1.f + lam[3] * expf(lam[4] * gamma)  + lam[5] * cosf(gamma) * cosf(gamma));
}

inline float RiAngleBetween(float thetav, float phiv, float theta, float phi) {
	const float cospsi = sinf(thetav) * sinf(theta) * cosf(phi - phiv) + cosf(thetav) * cosf(theta);
	if (cospsi >= 1.f)
		return 0.f;
	if (cospsi <= -1.f)
		return M_PI;
	return acosf(cospsi);
}

void ChromaticityToSpectrum(float Y, float x, float y, Spectrum *s) {
	float X, Z;
	
	if (y != 0.f)
		X = (x / y) * Y;
	else
		X = 0.f;
	
	if (y != 0.f && Y != 0.f)
		Z = (1.f - x - y) / y * Y;
	else
		Z = 0.f;

	// Assuming sRGB (D65 illuminant)
	s->r =  3.2410f * X - 1.5374f * Y - 0.4986f * Z;
	s->g = -0.9692f * X + 1.8760f * Y + 0.0416f * Z;
	s->b =  0.0556f * X - 0.2040f * Y + 1.0570f * Z;
}

SkyLight::SkyLight(float turb, const Vector &sd) : InfiniteLight(NULL) {
	turbidity = turb;
	sundir = Normalize(sd);
	gain = Spectrum(1.0f, 1.0f, 1.0f);

	Init();
}

void SkyLight::Init() {
	thetaS = SphericalTheta(sundir);
	phiS = SphericalPhi(sundir);

	float aconst = 1.0f;
	float bconst = 1.0f;
	float cconst = 1.0f;
	float dconst = 1.0f;
	float econst = 1.0f;

	float theta2 = thetaS*thetaS;
	float theta3 = theta2*thetaS;
	float T = turbidity;
	float T2 = T * T;

	float chi = (4.f / 9.f - T / 120.f) * (M_PI - 2.0f * thetaS);
	zenith_Y = (4.0453f * T - 4.9710f) * tan(chi) - 0.2155f * T + 2.4192f;
	zenith_Y *= 0.06f;

	zenith_x =
	(0.00166f * theta3 - 0.00375f * theta2 + 0.00209f * thetaS) * T2 +
	(-0.02903f * theta3 + 0.06377f * theta2 - 0.03202f * thetaS + 0.00394f) * T +
	(0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * thetaS + 0.25886f);

	zenith_y =
	(0.00275f * theta3 - 0.00610f * theta2 + 0.00317f * thetaS) * T2 +
	(-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * thetaS  + 0.00516f) * T +
	(0.15346f * theta3 - 0.26756f * theta2 + 0.06670f * thetaS  + 0.26688f);

	perez_Y[1] = (0.1787f * T  - 1.4630f) * aconst;
	perez_Y[2] = (-0.3554f * T  + 0.4275f) * bconst;
	perez_Y[3] = (-0.0227f * T  + 5.3251f) * cconst;
	perez_Y[4] = (0.1206f * T  - 2.5771f) * dconst;
	perez_Y[5] = (-0.0670f * T  + 0.3703f) * econst;

	perez_x[1] = (-0.0193f * T  - 0.2592f) * aconst;
	perez_x[2] = (-0.0665f * T  + 0.0008f) * bconst;
	perez_x[3] = (-0.0004f * T  + 0.2125f) * cconst;
	perez_x[4] = (-0.0641f * T  - 0.8989f) * dconst;
	perez_x[5] = (-0.0033f * T  + 0.0452f) * econst;

	perez_y[1] = (-0.0167f * T  - 0.2608f) * aconst;
	perez_y[2] = (-0.0950f * T  + 0.0092f) * bconst;
	perez_y[3] = (-0.0079f * T  + 0.2102f) * cconst;
	perez_y[4] = (-0.0441f * T  - 1.6537f) * dconst;
	perez_y[5] = (-0.0109f * T  + 0.0529f) * econst;

	zenith_Y /= PerezBase(perez_Y, 0, thetaS);
	zenith_x /= PerezBase(perez_x, 0, thetaS);
	zenith_y /= PerezBase(perez_y, 0, thetaS);
}

Spectrum SkyLight::Le(const Scene *scene, const Vector &dir) const {
	const float theta = SphericalTheta(dir);
	const float phi = SphericalPhi(dir);

	Spectrum s;
	GetSkySpectralRadiance(theta, phi, &s);

	return gain * s;
}

void SkyLight::GetSkySpectralRadiance(const float theta, const float phi, Spectrum * const spect) const {
	// add bottom half of hemisphere with horizon colour
	const float theta_fin = Min<float>(theta, (M_PI * 0.5f) - 0.001f);
	const float gamma = RiAngleBetween(theta, phi, thetaS, phiS);

	// Compute xyY values
	const float x = zenith_x * PerezBase(perez_x, theta_fin, gamma);
	const float y = zenith_y * PerezBase(perez_y, theta_fin, gamma);
	const float Y = zenith_Y * PerezBase(perez_Y, theta_fin, gamma);

	ChromaticityToSpectrum(Y, x, y, spect);
}

SunLight::SunLight(float turb, float size, const Vector &sd) : LightSource() {
	turbidity = turb;
	sundir = Normalize(sd);
	gain = Spectrum(1.0f, 1.0f, 1.0f);
	relSize = size;

	Init();
}

void SunLight::Init() {
	CoordinateSystem(sundir, &x, &y);

	// Values from NASA Solar System Exploration page
	// http://solarsystem.nasa.gov/planets/profile.cfm?Object=Sun&Display=Facts&System=Metric
	const float sunRadius = 695500.f;
	const float sunMeanDistance = 149600000.f;

	if (relSize * sunRadius <= sunMeanDistance) {
		sin2ThetaMax = relSize * sunRadius / sunMeanDistance;
		sin2ThetaMax *= sin2ThetaMax;
		cosThetaMax = sqrtf(1.f - sin2ThetaMax);
	} else {
		cosThetaMax = 0.f;
		sin2ThetaMax = 1.f;
	}

	Vector wh = Normalize(sundir);
	phiS = SphericalPhi(wh);
	thetaS = SphericalTheta(wh);

	// NOTE - lordcrc - sun_k_oWavelengths contains 64 elements, while sun_k_oAmplitudes contains 65?!?
	IrregularSPD k_oCurve(sun_k_oWavelengths, sun_k_oAmplitudes, 64);
	IrregularSPD k_gCurve(sun_k_gWavelengths, sun_k_gAmplitudes, 4);
	IrregularSPD k_waCurve(sun_k_waWavelengths, sun_k_waAmplitudes, 13);

	RegularSPD solCurve(sun_solAmplitudes, 380, 750, 38);  // every 5 nm

	float beta = 0.04608365822050f * turbidity - 0.04586025928522f;
	float tauR, tauA, tauO, tauG, tauWA;

	float m = 1.f / (cosf(thetaS) + 0.00094f * powf(1.6386f - thetaS, 
		-1.253f));  // Relative Optical Mass

	int i;
	float lambda;
	// NOTE - lordcrc - SPD stores data internally, no need for Ldata to stick around
	float Ldata[91];
	for(i = 0, lambda = 350.f; i < 91; ++i, lambda += 5.f) {
			// Rayleigh Scattering
		tauR = expf( -m * 0.008735f * powf(lambda / 1000.f, -4.08f));
			// Aerosol (water + dust) attenuation
			// beta - amount of aerosols present
			// alpha - ratio of small to large particle sizes. (0:4,usually 1.3)
		const float alpha = 1.3f;
		tauA = expf(-m * beta * powf(lambda / 1000.f, -alpha));  // lambda should be in um
			// Attenuation due to ozone absorption
			// lOzone - amount of ozone in cm(NTP)
		const float lOzone = .35f;
		tauO = expf(-m * k_oCurve.sample(lambda) * lOzone);
			// Attenuation due to mixed gases absorption
		tauG = expf(-1.41f * k_gCurve.sample(lambda) * m / powf(1.f +
			118.93f * k_gCurve.sample(lambda) * m, 0.45f));
			// Attenuation due to water vapor absorbtion
			// w - precipitable water vapor in centimeters (standard = 2)
		const float w = 2.0f;
		tauWA = expf(-0.2385f * k_waCurve.sample(lambda) * w * m /
		powf(1.f + 20.07f * k_waCurve.sample(lambda) * w * m, 0.45f));

		Ldata[i] = (solCurve.sample(lambda) *
			tauR * tauA * tauO * tauG * tauWA);
	}

	RegularSPD LSPD(Ldata, 350,800,91);
	// Note: (1000000000.0f / (M_PI * 100.f * 100.f)) is for compatibility with past scene
	suncolor = gain * LSPD.ToRGB() / (1000000000.0f / (M_PI * 100.f * 100.f));
}

void SunLight::SetGain(const Spectrum &g) {
	gain = g;
}

Spectrum SunLight::Le(const Scene *scene, const Vector &dir) const {
	if((cosThetaMax < 1.f) && (Dot(dir,-sundir) > cosThetaMax))
		return suncolor;
	else
		return Spectrum();
}

Spectrum SunLight::Sample_L(const Scene *scene, const Point &p, const Normal *N,
	const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	if (N && Dot(*N, -sundir) > 0.0f) {
		*pdf = 0.0f;
		return Spectrum();
	}

	Vector wi = UniformSampleCone(u0, u1, cosThetaMax, x, y, sundir);
	*shadowRay = Ray(p, wi);

	*pdf = UniformConePdf(cosThetaMax);

	return suncolor;
}

Spectrum SunLight::Sample_L(const Scene *scene, const float u0, const float u1,
		const float u2, const float u3, const float u4, float *pdf, Ray *ray) const {
	// Choose point on disk oriented toward infinite light direction
	const Point worldCenter = scene->dataSet->GetBSphere().center;
	const float worldRadius = scene->dataSet->GetBSphere().rad * 1.01f;

	float d1, d2;
	ConcentricSampleDisk(u0, u1, &d1, &d2);
	Point Pdisk = worldCenter + worldRadius * (d1 * x + d2 * y);

	// Set ray origin and direction for infinite light ray
	*ray = Ray(Pdisk + worldRadius * sundir, -UniformSampleCone(u2, u3, cosThetaMax, x, y, sundir));
	*pdf = UniformConePdf(cosThetaMax) / (M_PI * worldRadius * worldRadius);

	return suncolor;
}

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

InfiniteLight::InfiniteLight(TexMapInstance *tx) {
	tex = tx;
	shiftU = 0.f;
	shiftV = 0.f;
}

Spectrum InfiniteLight::Le(const Scene *scene, const Vector &dir) const {
	const UV uv(1.f - SphericalPhi(dir) * INV_TWOPI + shiftU, SphericalTheta(dir) * INV_PI + shiftV);
	return gain * tex->GetTexMap()->GetColor(uv);
}

Spectrum InfiniteLight::Sample_L(const Scene *scene, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	if (N) {
		Vector wi = CosineSampleHemisphere(u0, u1);
		*pdf = wi.z * INV_PI;

		Vector v1, v2;
		CoordinateSystem(Vector(*N), &v1, &v2);

		wi = Vector(
				v1.x * wi.x + v2.x * wi.y + N->x * wi.z,
				v1.y * wi.x + v2.y * wi.y + N->y * wi.z,
				v1.z * wi.x + v2.z * wi.y + N->z * wi.z);
		*shadowRay = Ray(p, wi);

		return Le(scene, wi);
	} else {
		Vector wi = UniformSampleSphere(u0, u1);

		*shadowRay = Ray(p, wi);
		*pdf = 1.f / (4.f * M_PI);

		return Le(scene, wi);
	}
}

Spectrum InfiniteLight::Sample_L(const Scene *scene, const float u0, const float u1,
		const float u2, const float u3, const float u4, float *pdf, Ray *ray) const {
	// Choose two points p1 and p2 on scene bounding sphere
	const Point worldCenter = scene->dataSet->GetBSphere().center;
	const float worldRadius = scene->dataSet->GetBSphere().rad * 1.01f;

	Point p1 = worldCenter + worldRadius * UniformSampleSphere(u0, u1);
	Point p2 = worldCenter + worldRadius * UniformSampleSphere(u2, u3);

	// Construct ray between p1 and p2
	*ray = Ray(p1, Normalize(p2 - p1));

	// Compute InfiniteAreaLight ray weight
	Vector toCenter = Normalize(worldCenter - p1);
	const float costheta = AbsDot(toCenter, ray->d);
	*pdf = costheta / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	return Le(scene, -ray->d);
}

//------------------------------------------------------------------------------
// InfiniteLight with portals
//------------------------------------------------------------------------------

InfiniteLightPortal::InfiniteLightPortal(TexMapInstance *tx, const std::string &portalFileName) :
	InfiniteLight(tx) {
	// Read portals
	SDL_LOG("Portal PLY objects file name: " << portalFileName);
	portals = ExtTriangleMesh::LoadExtTriangleMesh(portalFileName);
	const Triangle *tris = portals->GetTriangles();
	for (unsigned int i = 0; i < portals->GetTotalTriangleCount(); ++i)
		portalAreas.push_back(tris[i].Area(portals->GetVertices()));
}

InfiniteLightPortal::~InfiniteLightPortal() {
	portals->Delete();
	delete portals;
}

Spectrum InfiniteLightPortal::Sample_L(const Scene *scene, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	// Select one of the portals
	const unsigned int portalCount = portals->GetTotalTriangleCount();
	unsigned int portalIndex = Min<unsigned int>(Floor2UInt(portalCount * u2), portalCount - 1);

	// Looks for a valid portal
	for (unsigned int i = 0; i < portalCount; ++i) {
		// Sample the triangle
		Point samplePoint;
		float b0, b1, b2;
		portals->Sample(portalIndex, u0, u1, &samplePoint, &b0, &b1, &b2);
		const Normal sampleN = portals->GetGeometryNormal(portalIndex);

		// Check if the portal is visible
		Vector wi = samplePoint - p;
		const float distanceSquared = wi.LengthSquared();
		wi /= sqrtf(distanceSquared);

		const float sampleNdotMinusWi = Dot(sampleN, -wi);
		if ((sampleNdotMinusWi > 0.f) && (N && Dot(*N, wi) > 0.f)) {
			*shadowRay = Ray(p, wi);
			*pdf = distanceSquared / (sampleNdotMinusWi * portalAreas[portalIndex] * portalCount);

			// Using 0.1 instead of 0.0 to cut down fireflies
			if (*pdf <= 0.1f)
				continue;

			return Le(scene, wi);
		}

		if (++portalIndex >= portalCount)
			portalIndex = 0;
	}

	*pdf = 0.f;
	return Spectrum();
}

Spectrum InfiniteLightPortal::Sample_L(const Scene *scene, const float u0,
		const float u1, const float u2, const float u3, const float u4, float *pdf, Ray *ray) const {
	// Select one of the portals
	const unsigned int portalCount = portals->GetTotalTriangleCount();
	unsigned int portalIndex = Min<unsigned int>(Floor2UInt(portalCount * u4), portalCount - 1);

	// Sample the portal triangle
	Point samplePoint;
	float b0, b1, b2;
	portals->Sample(portalIndex, u0, u1, &samplePoint, &b0, &b1, &b2);
	const Normal &sampleN = portals->GetGeometryNormal(portalIndex);

	Vector wi = UniformSampleSphere(u2, u3);
	float RdotN = Dot(wi, sampleN);
	if (RdotN < 0.f) {
		wi *= -1.f;
		RdotN = -RdotN;
	}

	*ray = Ray(samplePoint, wi);
	*pdf = INV_TWOPI / (portalAreas[portalIndex] * portalCount);

	// Using 0.01 instead of 0.0 to cut down fireflies
	if (*pdf <= 0.01f) {
		*pdf = 0.f;
		return Spectrum();
	}

	return Le(scene, wi) * RdotN;
}

//------------------------------------------------------------------------------
// InfiniteLight with importance sampling
//------------------------------------------------------------------------------

InfiniteLightIS::InfiniteLightIS(TexMapInstance *tx) : InfiniteLight(tx) {
	uvDistrib = NULL;
}

void InfiniteLightIS::Preprocess() {
	// Build the importance map

	// Cale down the texture map
	const TextureMap *tm = tex->GetTexMap();
	const unsigned int nu = tm->GetWidth() / 2;
	const unsigned int nv = tm->GetHeight() / 2;
	//cerr << "Building importance sampling map for InfiniteLightIS: "<< nu << "x" << nv << endl;

	float *img = new float[nu * nv];
	UV uv;
	for (unsigned int v = 0; v < nv; ++v) {
		uv.v = (v + .5f) / nv + shiftV;

		for (unsigned int u = 0; u < nu; ++u) {
			uv.u = (u + .5f) / nu + shiftV;

			float pdf;
			LatLongMappingMap(uv.u, uv.v, NULL, &pdf);

			if (pdf > 0.f)
				img[u + v * nu] = tm->GetColor(uv).Filter() / pdf;
			else
				img[u + v * nu] = 0.f;
		}
	}

	uvDistrib = new Distribution2D(img, nu, nv);
	delete[] img;
}

Spectrum InfiniteLightIS::Sample_L(const Scene *scene, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	float uv[2];
	uvDistrib->SampleContinuous(u0, u1, uv, pdf);
	uv[0] -= shiftU;
	uv[1] -= shiftV;

	// Convert sample point to direction on the unit sphere
	const float phi = (1.f - uv[0]) * 2.f * M_PI;
	const float theta = uv[1] * M_PI;
	const float costheta = cosf(theta);
	const float sintheta = sinf(theta);
	const Vector wi = SphericalDirection(sintheta, costheta, phi);

	if (N && (Dot(wi, *N) <= 0.f)) {
		*pdf = 0.f;
		return Spectrum();
	}

	*shadowRay = Ray(p, wi);
	*pdf /= (2.f * M_PI * M_PI * sintheta);

	return gain * tex->GetTexMap()->GetColor(UV(uv));
}

Spectrum InfiniteLightIS::Sample_L(const Scene *scene, const float u0, const float u1,
		const float u2, const float u3, const float u4, float *pdf, Ray *ray) const {
	// Choose a point on scene bounding sphere using IS
	float uv[2];
	float mapPdf;
	uvDistrib->SampleContinuous(u0, u1, uv, &mapPdf);
	uv[0] -= shiftU;
	uv[1] -= shiftV;

    const float theta = uv[1] * M_PI;
	const float sintheta = sinf(theta);
	if (sintheta == 0.f) {
		*pdf = 0.f;
		return Spectrum();
	}
	const float costheta = cosf(theta);

	const float phi = (1.f - uv[0]) * 2.f * M_PI;
    const float sinphi = sinf(phi);
	const float cosphi = cosf(phi);
    Vector d = -Vector(sintheta * cosphi, sintheta * sinphi, costheta);

	// Compute origin for infinite light sample ray
	const Point worldCenter = scene->dataSet->GetBSphere().center;
	const float worldRadius = scene->dataSet->GetBSphere().rad * 1.01f;
    Vector v1, v2;
    CoordinateSystem(-d, &v1, &v2);

    float d1, d2;
    ConcentricSampleDisk(u2, u3, &d1, &d2);
    const Point Pdisk = worldCenter + worldRadius * (d1 * v1 + d2 * v2);

	*ray = Ray(Pdisk - worldRadius * d, d, 0.f, INFINITY);

    // Compute InfiniteAreaLight ray PDF
    const float directionPdf = mapPdf / (2.f * M_PI * M_PI * sintheta);
    const float areaPdf = 1.f / (M_PI * worldRadius * worldRadius);
    *pdf = directionPdf * areaPdf;

	return gain * tex->GetTexMap()->GetColor(UV(uv));
}

//------------------------------------------------------------------------------
// Triangle Area Light
//------------------------------------------------------------------------------

TriangleLight::TriangleLight(const AreaLightMaterial *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const std::vector<ExtMesh *> &objs) {
	lightMaterial = mat;
	meshIndex = mshIndex;
	triIndex = triangleIndex;

	Init(objs);
}

void TriangleLight::Init(const std::vector<ExtMesh *> &objs) {
	const ExtMesh *mesh = objs[meshIndex];
	area = mesh->GetTriangleArea(triIndex);
	invArea = 1.f / area;
}

Spectrum TriangleLight::Sample_L(const Scene *scene, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	const ExtMesh *mesh = scene->objects[meshIndex];

	Point samplePoint;
	float b0, b1, b2;
	mesh->Sample(triIndex, u0, u1, &samplePoint, &b0, &b1, &b2);
	const Normal &sampleN = mesh->GetGeometryNormal(triIndex);

	Vector wi = samplePoint - p;
	const float distanceSquared = wi.LengthSquared();
	const float distance = sqrtf(distanceSquared);
	wi /= distance;

	const float sampleNdotMinusWi = Dot(sampleN, -wi);
	if ((sampleNdotMinusWi <= 0.f) || (N && Dot(*N, wi) <= 0.f)) {
		*pdf = 0.f;
		return Spectrum();
	}

	*shadowRay = Ray(p, wi, MachineEpsilon::E(p), distance - MachineEpsilon::E(distance));
	*pdf = distanceSquared / (sampleNdotMinusWi * area);

	// Using 0.1 instead of 0.0 to cut down fireflies
	if (*pdf <= 0.1f) {
		*pdf = 0.f;
		return Spectrum();
	}

	if (mesh->HasColors())
		return mesh->GetColor(triIndex) * lightMaterial->GetGain(); // Light sources are supposed to have flat color
	else
		return lightMaterial->GetGain(); // Light sources are supposed to have flat color
}

Spectrum TriangleLight::Sample_L(const Scene *scene, const float u0, const float u1,
		const float u2, const float u3, const float u4, float *pdf, Ray *ray) const {
	const ExtMesh *mesh = scene->objects[meshIndex];

	// Ray origin
	float b0, b1, b2;
	Point orig;
	mesh->Sample(triIndex, u0, u1, &orig, &b0, &b1, &b2);

	// Ray direction
	const Normal &sampleN = mesh->GetGeometryNormal(triIndex); // Light sources are supposed to be flat
	Vector dir = UniformSampleSphere(u2, u3);
	float RdotN = Dot(dir, sampleN);
	if (RdotN < 0.f) {
		dir *= -1.f;
		RdotN = -RdotN;
	}

	*ray = Ray(orig, dir);

	*pdf = INV_TWOPI / area;

	if (mesh->HasColors())
		return mesh->GetColor(triIndex) * lightMaterial->GetGain() * RdotN; // Light sources are supposed to have flat color
	else
		return lightMaterial->GetGain() * RdotN; // Light sources are supposed to have flat color
}

Spectrum TriangleLight::Le(const Scene *scene, const Vector &dir) const {
	const ExtMesh *mesh = scene->objects[meshIndex];
	const Normal &sampleN = mesh->GetGeometryNormal(triIndex); // Light sources are supposed to be flat

	const float RdotN = Dot(-dir, sampleN);
	if (RdotN < 0.f)
		return Spectrum();

	if (mesh->HasColors())
		return M_PI * mesh->GetColor(triIndex) * lightMaterial->GetGain() * RdotN; // Light sources are supposed to have flat color
	else
		return M_PI * lightMaterial->GetGain() * RdotN; // Light sources are supposed to have flat color	
}

//------------------------------------------------------------------------------
// New interface
//------------------------------------------------------------------------------

Spectrum TriangleLight::Emit(const Scene *scene,
		const float u0, const float u1, const float u2, const float u3,
		Point *orig, Vector *dir, Normal *normal,
		float *emissionPdfW, float *directPdfA) const {
	const ExtMesh *mesh = scene->objects[meshIndex];

	// Origin
	float b0, b1, b2;
	mesh->Sample(triIndex, u0, u1, orig, &b0, &b1, &b2);

	// Build the local frame
	*normal = mesh->InterpolateTriNormal(triIndex, b1, b2);
	Frame frame(*normal);

	Vector localDirOut = CosineSampleHemisphere(u2, u3, emissionPdfW);
	*emissionPdfW *= invArea;

	// Cannot really not emit the particle, so just bias it to the correct angle
	localDirOut.z = Max(localDirOut.z, DEFAULT_EPSILON_STATIC);

	// Direction
	*dir = frame.ToWorld(localDirOut);

	if (directPdfA)
		*directPdfA = area;

	return lightMaterial->GetGain() * localDirOut.z;
}

Spectrum TriangleLight::GetRadiance(const Scene *scene,
		const Vector &dir,
		const Point &hitPoint,
		float *directPdfA,
		float *emissionPdfW) const {
	const ExtMesh *mesh = scene->objects[meshIndex];

	// Get the u and v coordinates of the hit point
	float b1, b2;
	if (!mesh->GetTriUV(triIndex, hitPoint, &b1, &b2))
		return Spectrum(0.f);

	// Build the local frame
	Frame frame(mesh->InterpolateTriNormal(triIndex, b1, b2));

	const float cosOutL = Max(0.f, Dot(frame.Normal(), -dir));

	if(cosOutL == 0.f)
		return Spectrum(0.f);

	if(directPdfA)
		*directPdfA = invArea;

	if(emissionPdfW)
		*emissionPdfW = cosOutL * INV_PI * invArea;

	return lightMaterial->GetGain();
}

Spectrum TriangleLight::Evaluate(const Scene *scene, const Vector &dir) const {
	const ExtMesh *mesh = scene->objects[meshIndex];
	const Normal &sampleN = mesh->GetGeometryNormal(triIndex); // Light sources are supposed to be flat

	const float RdotN = Dot(-dir, sampleN);
	if (RdotN < 0.f)
		return Spectrum();

	if (mesh->HasColors())
		return M_PI * mesh->GetColor(triIndex) * lightMaterial->GetGain() * RdotN; // Light sources are supposed to have flat color
	else
		return M_PI * lightMaterial->GetGain() * RdotN; // Light sources are supposed to have flat color	
}
