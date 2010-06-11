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

// Based on smallppm, Progressive Photon Mapping by T. Hachisuka

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#include <stdexcept>

#include "smallsppmgpu.h"
#include "displayfunc.h"

std::string SPPMG_LABEL = "LuxRays SmallSPPMGPU v" LUXRAYS_VERSION_MAJOR "." LUXRAYS_VERSION_MINOR " (LuxRays demo: http://www.luxrender.net)";

luxrays::Context *ctx = NULL;
luxrays::sdl::Scene *scene = NULL;

double startTime = 0.0;
unsigned int scrRefreshInterval = 2000;

unsigned int imgWidth = 640;
unsigned int imgHeight = 480;
std::string imgFileName = "image.png";
float photonAlpha = 0.7f;
unsigned int stochasticInterval = 2000000;

luxrays::utils::Film *film = NULL;
luxrays::SampleBuffer *sampleBuffer = NULL;

boost::thread *renderThread = NULL;
unsigned long long photonTraced = 0;

HitPoints *hitPoints = NULL;

//------------------------------------------------------------------------------

void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** ");
	if(fif != FIF_UNKNOWN)
		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));

	printf("%s", message);
	printf(" ***\n");
}

void DebugHandler(const char *msg) {
	std::cerr << msg << std::endl;
}

//------------------------------------------------------------------------------

static void InitPhotonPath(luxrays::sdl::Scene *scene, luxrays::RandomGenerator *rndGen,
	PhotonPath *photonPath, luxrays::Ray *ray) {
	// Select one light source
	float lpdf;
	const luxrays::sdl::LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lpdf);

	// Initialize the photon path
	float pdf;
	photonPath->flux = light->Sample_L(scene,
		rndGen->floatValue(), rndGen->floatValue(),
		rndGen->floatValue(), rndGen->floatValue(),
		&pdf, ray);
	photonPath->flux /= pdf * lpdf;
	photonPath->depth = 0;

	photonTraced++;
}

static void TracePhotonsThread(luxrays::RandomGenerator *rndGen, luxrays::IntersectionDevice *device) {
	std::cerr << "Tracing photon paths" << std::endl;

	luxrays::RayBuffer *rayBuffer = device->NewRayBuffer();

	// Generate photons from light sources
	std::vector<PhotonPath> photonPaths(rayBuffer->GetSize());
	luxrays::Ray *rays = rayBuffer->GetRayBuffer();
	for (unsigned int i = 0; i < photonPaths.size(); ++i) {
		// Note: there is some assumption here about how the
		// rayBuffer->ReserveRay() work
		InitPhotonPath(scene, rndGen, &photonPaths[i], &rays[rayBuffer->ReserveRay()]);
	}

	startTime = luxrays::WallClockTime();
	unsigned long long lastEyePathRecast = photonTraced;
	while (!boost::this_thread::interruption_requested()) {
		// Trace the rays
		device->PushRayBuffer(rayBuffer);
		rayBuffer = device->PopRayBuffer();

		for (unsigned int i = 0; i < rayBuffer->GetRayCount(); ++i) {
			PhotonPath *photonPath = &photonPaths[i];
			luxrays::Ray *ray = &rayBuffer->GetRayBuffer()[i];
			const luxrays::RayHit *rayHit = &rayBuffer->GetHitBuffer()[i];

			if (rayHit->Miss()) {
				// Re-initialize the photon path
				InitPhotonPath(scene, rndGen, photonPath, ray);
			} else {
				// Something was hit
				luxrays::Point hitPoint;
				luxrays::Spectrum surfaceColor;
				luxrays::Normal N, shadeN;
				if (GetHitPointInformation(scene, rndGen, ray, rayHit, hitPoint,
						surfaceColor, N, shadeN))
					continue;

				// Get the material
				const unsigned int currentTriangleIndex = rayHit->index;
				const luxrays::sdl::Material *triMat = scene->triangleMaterials[currentTriangleIndex];

				if (triMat->IsLightSource()) {
					// Re-initialize the photon path
					InitPhotonPath(scene, rndGen, photonPath, ray);
				} else {
					const luxrays::sdl::SurfaceMaterial *triSurfMat = (luxrays::sdl::SurfaceMaterial *)triMat;

					float fPdf;
					luxrays::Vector wi;
					bool specularBounce;
					const luxrays::Spectrum f = triSurfMat->Sample_f(-ray->d, &wi, N, shadeN,
							rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
							false, &fPdf, specularBounce) * surfaceColor;

					if (!specularBounce)
						hitPoints->AddFlux(photonAlpha, hitPoint, shadeN, -ray->d, photonPath->flux);

					// Check if we reached the max. depth
					if (photonPath->depth < MAX_PHOTON_PATH_DEPTH) {
						// Build the next vertex path ray
						if ((fPdf <= 0.f) || f.Black()) {
							// Re-initialize the photon path
							InitPhotonPath(scene, rndGen, photonPath, ray);
						} else {
							photonPath->depth++;
							photonPath->flux *= f / fPdf;

							// Russian Roulette
							const float p = 0.75f;
							if (photonPath->depth < 3) {
								*ray = luxrays::Ray(hitPoint, wi);
							} else if (rndGen->floatValue() < p) {
								photonPath->flux /= p;
								*ray = luxrays::Ray(hitPoint, wi);
							} else {
								// Re-initialize the photon path
								InitPhotonPath(scene, rndGen, photonPath, ray);
							}
						}
					} else {
						// Re-initialize the photon path
						InitPhotonPath(scene, rndGen, photonPath, ray);
					}
				}
			}
		}

		// Check if it is time to do an HashGrid re-hash
		// TODO: add a parameter to tune rehashing intervals
		if (photonTraced - lastEyePathRecast > stochasticInterval) {
			hitPoints->Recast(photonTraced);

			lastEyePathRecast = photonTraced;
			std::cerr << "Tracing photon paths" << std::endl;
		}
	}

	delete rayBuffer;
	delete rndGen;
}

//------------------------------------------------------------------------------

int main(int argc, char *argv[]) {
	std::cerr << SPPMG_LABEL << std::endl;

	try {
		//----------------------------------------------------------------------
		// Parse command line options
		//----------------------------------------------------------------------

		std::cerr << "Usage: " << argv[0] << " [options] [scene file]" << std::endl <<
				" -w [image width]" << std::endl <<
				" -e [image height]" << std::endl <<
				" -a [photon alpha]" << std::endl <<
				" -r [screen refresh interval]" << std::endl <<
				" -i [image file name]" << std::endl <<
				" -s [stochastic photon count refresh]" << std::endl <<
				" -h <display this help and exit>" << std::endl;

		std::string sceneFileName = "scenes/luxball/luxball.scn";
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				// I should check for out of range array index...

				if (argv[i][1] == 'h') exit(EXIT_SUCCESS);

				else if (argv[i][1] == 'e') imgHeight = atoi(argv[++i]);

				else if (argv[i][1] == 'w') imgWidth = atoi(argv[++i]);

				else if (argv[i][1] == 'a') photonAlpha = atof(argv[++i]);

				else if (argv[i][1] == 'r') scrRefreshInterval = atoi(argv[++i]);

				else if (argv[i][1] == 'i') imgFileName = argv[++i];

				else if (argv[i][1] == 's') stochasticInterval = atoi(argv[++i]);

				else {
					std::cerr << "Invalid option: " << argv[i] << std::endl;
					exit(EXIT_FAILURE);
				}
			} else {
				std::string s = argv[i];
				if ((s.length() >= 4) && (s.substr(s.length() - 4) == ".scn")) {
					sceneFileName = s;
				} else
					throw std::runtime_error("Unknow file extension: " + s);
			}
		}

		//--------------------------------------------------------------------------
		// Init GLUT
		//--------------------------------------------------------------------------

		InitGlut(argc, argv, imgWidth, imgHeight);

		//--------------------------------------------------------------------------
		// Create the LuxRays context
		//--------------------------------------------------------------------------

		ctx = new luxrays::Context(DebugHandler);

		// Looks for the first GPU device
		std::vector<luxrays::DeviceDescription *> interDevDescs = std::vector<luxrays::DeviceDescription *>(ctx->GetAvailableDeviceDescriptions());
		//luxrays::DeviceDescription::FilterOne(interDevDescs);

		luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_NATIVE_THREAD, interDevDescs);

		//luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL, interDevDescs);
		//luxrays::OpenCLDeviceDescription::Filter(luxrays::OCL_DEVICE_TYPE_GPU, interDevDescs);

		if (interDevDescs.size() < 1) {
			std::cerr << "Unable to find a GPU or CPU intersection device" << std::endl;
			return (EXIT_FAILURE);
		}
		interDevDescs.resize(1);

		std::cerr << "Selected intersection device: " << interDevDescs[0]->GetName();
		std::vector<luxrays::IntersectionDevice *> devices = ctx->AddIntersectionDevices(interDevDescs);
		luxrays::IntersectionDevice *device = devices[0];

		luxrays::RandomGenerator *rndGen = new luxrays::RandomGenerator();
		rndGen->init(7);

		//----------------------------------------------------------------------
		// Allocate the Film
		//----------------------------------------------------------------------

		std::vector<luxrays::DeviceDescription *> pixelDevDecs = ctx->GetAvailableDeviceDescriptions();
		luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_NATIVE_THREAD, pixelDevDecs);
		film = new luxrays::utils::LuxRaysFilm(ctx, imgWidth, imgHeight, pixelDevDecs[0]);
		//film->SetFilterType(luxrays::FILTER_NONE);
		sampleBuffer = film->GetFreeSampleBuffer();

		//----------------------------------------------------------------------
		// Read the scene
		//----------------------------------------------------------------------

		scene = new luxrays::sdl::Scene(ctx, sceneFileName);
		scene->camera->Update(imgWidth, imgHeight);

		// Set the Luxrays SataSet
		ctx->SetDataSet(scene->dataSet);
		ctx->Start();

		//----------------------------------------------------------------------
		// Build the EyePaths list
		//----------------------------------------------------------------------

		hitPoints = new HitPoints(scene, rndGen, device, photonAlpha, imgWidth, imgHeight);

		//----------------------------------------------------------------------
		// Start photon tracing thread
		//----------------------------------------------------------------------

		renderThread = new boost::thread(boost::bind(TracePhotonsThread, rndGen, device));

		//----------------------------------------------------------------------
		// Start GLUT loop
		//----------------------------------------------------------------------

		RunGlut(imgWidth, imgHeight);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	} catch (cl::Error err) {
		std::cerr << "OpenCL ERROR: " << err.what() << "(" << err.err() << ")" << std::endl;
#endif
	} catch (std::runtime_error err) {
		std::cerr << "RUNTIME ERROR: " << err.what() << std::endl;
	} catch (std::exception err) {
		std::cerr << "ERROR: " << err.what() << std::endl;
	}

	return EXIT_SUCCESS;
}
