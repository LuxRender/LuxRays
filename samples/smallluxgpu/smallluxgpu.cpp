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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include "smalllux.h"
#include "displayfunc.h"
#include "renderconfig.h"
#include "path.h"
#include "telnet.h"
#include "luxrays/core/device.h"

#if defined(__GNUC__)
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <typeinfo>
#include <cxxabi.h>

static string Demangle(const char *symbol) {
	size_t size;
	int status;
	char temp[128];
	char* result;

	if (1 == sscanf(symbol, "%*[^'(']%*[^'_']%[^')''+']", temp)) {
		if (NULL != (result = abi::__cxa_demangle(temp, NULL, &size, &status))) {
			string r = result;
			return r + " [" + symbol + "]";
		}
	}

	if (1 == sscanf(symbol, "%127s", temp))
		return temp;

	return symbol;
}

void SLGTerminate(void) {
	cerr << "=========================================================" << endl;
	cerr << "Unhandled exception" << endl;

	void *array[32];
	size_t size = backtrace(array, 32);
	char **strings = backtrace_symbols(array, size);

	cerr << "Obtained " << size << " stack frames." << endl;

	for (size_t i = 0; i < size; i++)
		cerr << "  " << Demangle(strings[i]) << endl;

	free(strings);
}
#endif

void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** ");
	if(fif != FIF_UNKNOWN)
		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));

	printf("%s", message);
	printf(" ***\n");
}

static int BatchMode(double stopTime, unsigned int stopSPP) {
	const double startTime = WallClockTime();

	double sampleSec = 0.0;
	double lastTimeSave = startTime;
	const double periodiceSaveTime = config->cfg.GetFloat("batch.periodicsave", 0.f);
	const vector<string> filmNames = config->cfg.GetStringVector("screen.file", "");
	const string fileName = config->cfg.GetString("image.filename", "image.png");
	char buff[512];
	const vector<IntersectionDevice *> interscetionDevices = config->GetIntersectionDevices();
	for (;;) {
		boost::this_thread::sleep(boost::posix_time::millisec(1000));
		const double now = WallClockTime();
		const double elapsedTime =now - startTime;

		unsigned int pass = 0;
		const vector<RenderThread *> &renderThreads = config->GetRenderThreads();
		for (size_t i = 0; i < renderThreads.size(); ++i)
			pass += renderThreads[i]->GetPass();

		if ((stopTime > 0) && (elapsedTime >= stopTime))
			break;
		if ((stopSPP > 0) && (pass >= stopSPP))
			break;

		// Check if periodic save is enabled
		if (periodiceSaveTime > 0.) {
			if (now - lastTimeSave > periodiceSaveTime) {
				// Time to save the image and film
				config->scene->camera->film->UpdateScreenBuffer();
				config->scene->camera->film->Save(fileName);

				if (filmNames.size() == 1)
					config->scene->camera->film->SaveFilm(filmNames[0]);
				else if (filmNames.size() > 1)
					config->scene->camera->film->SaveFilm("merged.flm");

				lastTimeSave = WallClockTime();
			}
		}

		double raysSec = 0.0;
		for (size_t i = 0; i < interscetionDevices.size(); ++i)
			raysSec += interscetionDevices[i]->GetPerformance();

		sampleSec = config->scene->camera->film->GetAvgSampleSec();
		sprintf(buff, "[Elapsed time: %3d/%dsec][Samples %4d/%d][Avg. samples/sec % 4dK][Avg. rays/sec % 4dK on %.1fK tris]",
				int(elapsedTime), int(stopTime), pass, stopSPP, int(sampleSec/ 1000.0),
				int(raysSec / 1000.0), config->scene->dataSet->GetTotalTriangleCount() / 1000.0);
		cerr << buff << endl;
	}

	// Save the rendered image
	config->scene->camera->film->UpdateScreenBuffer();
	config->scene->camera->film->Save(fileName);

	// Check if I have to save the film
	if (filmNames.size() == 1)
		config->scene->camera->film->SaveFilm(filmNames[0]);
	else if (filmNames.size() > 1)
		config->scene->camera->film->SaveFilm("merged.flm");

	sprintf(buff, "LuxMark index: %.3f", sampleSec / 1000000.0);
	cerr << buff << endl;

	delete config;
	cerr << "Done." << endl;

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
#if defined(__GNUC__)
	set_terminate(SLGTerminate);
#endif

	try {
		cerr << "Usage: " << argv[0] << " [options] [configuration file]" << endl <<
				" -o [configuration file]" << endl <<
				" -f [scene file]" << endl <<
				" -w [window width]" << endl <<
				" -e [window height]" << endl <<
				" -g <disable OpenCL GPU device>" << endl <<
				" -p <enable OpenCL CPU device>" << endl <<
				" -n [native thread count]" << endl <<
				" -r [gpu thread count]" << endl <<
				" -l [set high/low latency mode]" << endl <<
				" -b <enable high latency mode>" << endl <<
				" -s [GPU workgroup size]" << endl <<
				" -t [halt time in secs]" << endl <<
				" -T <enable the telnet server>" << endl <<
				" -D [property name] [property value]" << endl <<
				" -d [current directory path]" << endl <<
				" -h <display this help and exit>" << endl;

		// Initialize FreeImage Library
		FreeImage_Initialise(TRUE);
		FreeImage_SetOutputMessage(FreeImageErrorHandler);

		bool batchMode = false;
		bool telnetServerEnabled = false;
		Properties cmdLineProp;
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				// I should check for out of range array index...

				if (argv[i][1] == 'h') exit(EXIT_SUCCESS);

				else if (argv[i][1] == 'o') {
					if (config)
						throw runtime_error("Used multiple configuration files");

					config = new RenderingConfig(argv[++i]);
				}

				else if (argv[i][1] == 'e') cmdLineProp.SetString("image.height", argv[++i]);

				else if (argv[i][1] == 'w') cmdLineProp.SetString("image.width", argv[++i]);

				else if (argv[i][1] == 'f') cmdLineProp.SetString("scene.file", argv[++i]);

				else if (argv[i][1] == 'p') cmdLineProp.SetString("opencl.cpu.use", "1");

				else if (argv[i][1] == 'g') cmdLineProp.SetString("opencl.gpu.use", "0");

				else if (argv[i][1] == 'l') cmdLineProp.SetString("opencl.latency.mode", argv[++i]);

				else if (argv[i][1] == 'n') cmdLineProp.SetString("opencl.nativethread.count", argv[++i]);

				else if (argv[i][1] == 'r') cmdLineProp.SetString("opencl.renderthread.count", argv[++i]);

				else if (argv[i][1] == 's') cmdLineProp.SetString("opencl.gpu.workgroup.size", argv[++i]);

				else if (argv[i][1] == 't') cmdLineProp.SetString("batch.halttime", argv[++i]);

				else if (argv[i][1] == 'T') telnetServerEnabled = true;

				else if (argv[i][1] == 'D') {
					cmdLineProp.SetString(argv[i + 1], argv[i + 2]);
					i += 2;
				}

				else if (argv[i][1] == 'd') boost::filesystem::current_path(boost::filesystem::path(argv[++i]));

				else {
					cerr << "Invalid option: " << argv[i] << endl;
					exit(EXIT_FAILURE);
				}
			} else {
				string s = argv[i];
				if ((s.length() >= 4) && (s.substr(s.length() - 4) == ".cfg")) {
					if (config)
						throw runtime_error("Used multiple configuration files");
					config = new RenderingConfig(s);
				} else
					throw runtime_error("Unknow file extension: " + s);
			}
		}

		if (!config)
			config = new RenderingConfig("scenes/luxball/render-fast.cfg");

		config->cfg.Load(cmdLineProp);

		const unsigned int halttime = config->cfg.GetInt("batch.halttime", 0);
		const unsigned int haltspp = config->cfg.GetInt("batch.haltspp", 0);
		if ((halttime > 0) || (haltspp > 0))
			batchMode = true;
		else
			batchMode = false;

		if (batchMode) {
			config->Init();
			return BatchMode(halttime, haltspp);
		} else {
			// It is important to initialize OpenGL before OpenCL
			unsigned int width = config->cfg.GetInt("image.width", 640);
			unsigned int height = config->cfg.GetInt("image.height", 480);

			InitGlut(argc, argv, width, height);

			config->Init();

			if (telnetServerEnabled) {
				TelnetServer telnetServer(18081, config);
				RunGlut();
			} else
				RunGlut();
		}
#if !defined(LUXRAYS_DISABLE_OPENCL)
	} catch (cl::Error err) {
		cerr << "OpenCL ERROR: " << err.what() << "(" << err.err() << ")" << endl;
#endif
	} catch (runtime_error err) {
		cerr << "RUNTIME ERROR: " << err.what() << endl;
	} catch (exception err) {
		cerr << "ERROR: " << err.what() << endl;
	}

	return EXIT_SUCCESS;
}
