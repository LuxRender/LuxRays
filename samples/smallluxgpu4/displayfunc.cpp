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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32)
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#include <boost/format.hpp>

// Jens's patch for MacOS
#if defined(__APPLE__)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "smallluxgpu.h"
#include "displayfunc.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/virtualdevice.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

static void PrintString(void *font, const char *string) {
	int len, i;

	len = (int)strlen(string);
	for (i = 0; i < len; i++)
		glutBitmapCharacter(font, string[i]);
}

static void PrintHelpString(const u_int x, const u_int y, const char *key, const char *msg) {
	glColor3f(0.9f, 0.9f, 0.5f);
	glRasterPos2i(x, y);
	PrintString(GLUT_BITMAP_8_BY_13, key);

	glColor3f(1.f, 1.f, 1.f);
	// To update raster color
	glRasterPos2i(x + glutBitmapLength(GLUT_BITMAP_8_BY_13, (unsigned char *)key), y);
	PrintString(GLUT_BITMAP_8_BY_13, ": ");
	PrintString(GLUT_BITMAP_8_BY_13, msg);
}

static void PrintHelpAndSettings() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.5f);
	glRecti(10, 20, session->GetFilm().GetWidth() - 10, session->GetFilm().GetHeight() - 20);
	glDisable(GL_BLEND);

	glColor3f(1.f, 1.f, 1.f);
	int fontOffset = session->GetFilm().GetHeight() - 20 - 20;
	glRasterPos2i((session->GetFilm().GetWidth() - glutBitmapLength(GLUT_BITMAP_9_BY_15, (unsigned char *)"Help & Settings & Devices")) / 2, fontOffset);
	PrintString(GLUT_BITMAP_9_BY_15, "Help & Settings & Devices");

	// Help
	fontOffset -= 25;
	PrintHelpString(15, fontOffset, "h", "toggle Help");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "arrow Keys or mouse X/Y + mouse button 0", "rotate camera");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "a, s, d, w or mouse X/Y + mouse button 1", "move camera");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "p", "save image.png (or to image.filename property value)");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "t", "toggle tonemapping");
	PrintHelpString(320, fontOffset, "n, m", "dec./inc. the screen refresh");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "i", "switch sampler");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "1", "OpenCL path tracing");
	PrintHelpString(320, fontOffset, "2", "CPU light tracing");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "3", "CPU path tracing");
	PrintHelpString(320, fontOffset, "4", "CPU bidir. path tracing");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "5", "Hybrid bidir. path tracing");
	PrintHelpString(320, fontOffset, "6", "Hybrid combinatorial bidir. path tracing");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "7", "CPU bidir. VM path tracing");
	PrintHelpString(320, fontOffset, "8", "RT OpenCL path tracing");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "9", "Hybrid path tracing");
	fontOffset -= 15;
#if defined(WIN32)
	PrintHelpString(15, fontOffset, "o", "windows always on top");
	fontOffset -= 15;
#endif

	// Settings
	string buffer;
	glColor3f(0.5f, 1.0f, 0.f);
	fontOffset -= 15;
	glRasterPos2i(15, fontOffset);
	PrintString(GLUT_BITMAP_8_BY_13, "Settings:");
	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);

	const Properties &stats = session->GetStats();
	const string engineType = config->GetProperty("renderengine.type").Get<string>();
#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (engineType == "RTPATHOCL") {
		static float fps = 0.f;
		// This is a simple trick to smooth the fps counter
		const double frameTime = stats.Get("stats.rtpathocl.frame.time").Get<double>();
		const double adjustFactor = (frameTime > 0.1) ? 0.25 : .025;
		fps = Lerp<float>(adjustFactor, fps, (frameTime > 0.0) ? (1.0 / frameTime) : 0.0);

		buffer = boost::str(boost::format("[Rendering time %dsecs][Screen refresh %d/%dms %.1ffps]") %
				int(stats.Get("stats.renderengine.time").Get<double>()) %
				int((fps > 0.f) ? (1000.0 / fps) : 0.0) %
				config->GetProperty("screen.refresh.interval").Get<u_int>() %
				fps);
	} else if (engineType == "RTBIASPATHOCL") {
		static float fps = 0.f;
		// This is a simple trick to smooth the fps counter
		const double frameTime = stats.Get("stats.rtbiaspathocl.frame.time").Get<double>();
		const double adjustFactor = (frameTime > 0.1) ? 0.25 : .025;
		fps = Lerp<float>(adjustFactor, fps, (frameTime > 0.0) ? (1.0 / frameTime) : 0.0);

		buffer = boost::str(boost::format("[Rendering time %dsecs][Screen refresh %dms %.1ffps]") %
				int(stats.Get("stats.renderengine.time").Get<double>()) %
				int((fps > 0.f) ? (1000.0 / fps) : 0.0) %
				fps);
	} else
#endif
		buffer = boost::str(boost::format("[Rendering time %dsecs][Screen refresh %dms]") %
				(stats.Get("stats.renderengine.time").Get<double>()) %
				config->GetProperty("screen.refresh.interval").Get<u_int>());
	PrintString(GLUT_BITMAP_8_BY_13, buffer.c_str());

	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);
	const string samplerName = ((engineType == "BIASPATHCPU") ||
		(engineType == "RTBIASPATHOCL")) ?
			"N/A" : config->GetProperty("sampler.type").Get<string>();
	buffer = boost::str(boost::format("[Render engine %s][Sampler %s][Tone mapping %s]") %
			engineType.c_str() % samplerName.c_str() %
			ToneMapType2String(String2ToneMapType(config->GetProperty("film.tonemap.type").Get<string>())));
	PrintString(GLUT_BITMAP_8_BY_13, buffer.c_str());
	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);

	// Intersection devices
	const Property &deviceNames = stats.Get("stats.renderengine.devices");
	glColor3f(1.0f, 0.5f, 0.f);
	int offset = 45;

	double minPerf = 0.0;
	double totalPerf = 0.0;
	for (u_int i = 0; i < deviceNames.GetSize(); ++i) {
		const string deviceName = deviceNames.Get<string>(i);

		const double perf = stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>();
		minPerf = Min(minPerf, perf);
		totalPerf += perf;
	}

	for (u_int i = 0; i < deviceNames.GetSize(); ++i) {
		const string deviceName = deviceNames.Get<string>(i);

		buffer = boost::str(boost::format("[%s][Rays/sec %dK (%dK + %dK)][Prf Idx %.2f][Wrkld %.1f%%][Mem %dM/%dM]") %
			deviceName.c_str() %
			int(stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>() / 1000.0) %
			int(stats.Get("stats.renderengine.devices." + deviceName + ".performance.serial").Get<double>() / 1000.0) %
			int(stats.Get("stats.renderengine.devices." + deviceName + ".performance.dataparallel").Get<double>() / 1000.0) %
			(stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>() / minPerf) %
			(100.0 * stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>() / totalPerf) %
			int(stats.Get("stats.renderengine.devices." + deviceName + ".memory.used").Get<size_t>() / (1024 * 1024)) %
			int(stats.Get("stats.renderengine.devices." + deviceName + ".memory.total").Get<size_t>() / (1024 * 1024)));

		glRasterPos2i(20, offset);
		PrintString(GLUT_BITMAP_8_BY_13, buffer.c_str());
		offset += 15;
	}

	glRasterPos2i(15, offset);
	PrintString(GLUT_BITMAP_9_BY_15, "Rendering devices:");
}

static void PrintCaptions() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.8f);
	glRecti(0, session->GetFilm().GetHeight() - 15,
			session->GetFilm().GetWidth() - 1, session->GetFilm().GetHeight() - 1);
	glRecti(0, 0, session->GetFilm().GetWidth() - 1, 18);
	glDisable(GL_BLEND);

	// Draw the pending tiles for BIASPATHCPU or BIASPATHOCL
	const Properties &stats = session->GetStats();
	const string engineType = config->GetProperty("renderengine.type").Get<string>();
	if ((engineType == "BIASPATHCPU") || (engineType == "BIASPATHOCL")) {
		const u_int tileSize = stats.Get("stats.biaspath.tiles.size").Get<u_int>();
		const u_int tileCount = stats.Get("stats.biaspath.tiles.pending.count").Get<u_int>();

		// Draw tiles borders
		const Property &prop = stats.Get("stats.biaspath.tiles.pending.coords");
		glColor3f(1.f, 1.f, 0.f);
		for (u_int i = 0; i < tileCount; ++i) {
			const u_int xStart = prop.Get<u_int>(i * 2);
			const u_int yStart = prop.Get<u_int>(i * 2 + 1);

			glBegin(GL_LINE_LOOP);
			glVertex2i(xStart, yStart);
			glVertex2i(xStart + tileSize, yStart);
			glVertex2i(xStart + tileSize, yStart + tileSize);
			glVertex2i(xStart, yStart + tileSize);
			glEnd();
		}
	}

	const string buffer = boost::str(boost::format("[Pass %3d][Avg. samples/sec % 3.2fM][Avg. rays/sec % 4dK on %.1fK tris]") %
		stats.Get("stats.renderengine.pass").Get<int>() %
		(stats.Get("stats.renderengine.total.samplesec").Get<double>() / 1000000.0) %
		int(stats.Get("stats.renderengine.performance.total").Get<double>() / 1000.0) %
		(stats.Get("stats.dataset.trianglecount").Get<size_t>() / 1000.0));

	// Caption line 0
	glColor3f(1.f, 1.f, 1.f);
	glRasterPos2i(4, 5);
	PrintString(GLUT_BITMAP_8_BY_13, buffer.c_str());

	// Title
	glRasterPos2i(4, session->GetFilm().GetHeight() - 10);
	if (optUseLuxVRName)
		PrintString(GLUT_BITMAP_8_BY_13, slg::LUXVR_LABEL.c_str());
	else
		PrintString(GLUT_BITMAP_8_BY_13, slg::SLG_LABEL.c_str());
}

void displayFunc(void) {
	const float *pixels = session->GetFilm().GetChannel<float>(Film::CHANNEL_RGB_TONEMAPPED);

	glRasterPos2i(0, 0);
	glDrawPixels(session->GetFilm().GetWidth(), session->GetFilm().GetHeight(), GL_RGB, GL_FLOAT, pixels);

	session->UpdateStats();
	PrintCaptions();

	if (optOSDPrintHelp) {
		glPushMatrix();
		glLoadIdentity();
		glOrtho(-.5f, session->GetFilm().GetWidth() - .5f,
				-.5f, session->GetFilm().GetHeight() - .5f, -1.f, 1.f);

		PrintHelpAndSettings();

		glPopMatrix();
	}

	glutSwapBuffers();
}

void idleFunc(void) {
	session->WaitNewFrame();
	displayFunc();
}

void reshapeFunc(int newWidth, int newHeight) {
	// Check if width or height have really changed
	if ((newWidth != (int)session->GetFilm().GetWidth()) ||
			(newHeight != (int)session->GetFilm().GetHeight())) {
		glViewport(0, 0, newWidth, newHeight);
		glLoadIdentity();
		glOrtho(0.f, newWidth - 1.f, 0.f, newHeight - 1.f, -1.f, 1.f);

		// Stop the session
		session->Stop();

		// Delete the session
		delete session;
		session = NULL;

		// Change the film size
		config->Parse(
				Property("film.width")(newWidth) <<
				Property("film.height")(newHeight));
		session = new RenderSession(config);

		// Re-start the rendering
		session->Start();

		glutPostRedisplay();
	}
}

void timerFunc(int value) {
	// Check if periodic save is enabled
	if (session->NeedPeriodicFilmSave()) {
		// Time to save the image and film
		session->GetFilm().Save();
	}

	glutPostRedisplay();
	if (!optRealTimeMode)
		glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
}

static void SetRenderingEngineType(const string &engineType) {
	if (engineType != config->GetProperty("renderengine.type").Get<string>()) {
		// Stop the session
		session->Stop();

		// Delete the session
		delete session;
		session = NULL;

		// Change the render engine
		config->Parse(
				Properties() <<
				Property("renderengine.type")(engineType));
		session = new RenderSession(config);

		// Re-start the rendering
		session->Start();
	}
}

void keyFunc(unsigned char key, int x, int y) {
	switch (key) {
		case 'p': {
			session->GetFilm().Save();
			break;
		}
		case 27: { // Escape key
			delete session;

			SLG_LOG("Done.");
			exit(EXIT_SUCCESS);
			break;
		}
		case ' ': // Restart rendering
			session->Stop();
			session->Start();
			break;
		case 'a': {
			session->BeginSceneEdit();
			config->GetScene().GetCamera().TranslateLeft(optMoveStep);
			session->EndSceneEdit();
			break;
		}
		case 'd': {
			session->BeginSceneEdit();
			config->GetScene().GetCamera().TranslateRight(optMoveStep);
			session->EndSceneEdit();
			break;
		}
		case 'w': {
			session->BeginSceneEdit();
			config->GetScene().GetCamera().TranslateForward(optMoveStep);
			session->EndSceneEdit();
			break;
		}
		case 's': {
			session->BeginSceneEdit();
			config->GetScene().GetCamera().TranslateBackward(optMoveStep);
			session->EndSceneEdit();
			break;
		}
		case 'r':
			session->BeginSceneEdit();
			config->GetScene().GetCamera().Translate(Vector(0.f, 0.f, optMoveStep));
			session->EndSceneEdit();
			break;
		case 'f':
			session->BeginSceneEdit();
			config->GetScene().GetCamera().Translate(Vector(0.f, 0.f, -optMoveStep));
			session->EndSceneEdit();
			break;
		case 'h':
			optOSDPrintHelp = (!optOSDPrintHelp);
			break;
		case 'i': {
			// Stop the session
			session->Stop();

			// Delete the session
			delete session;
			session = NULL;

			// Change the Sampler
			const string samplerName = config->GetProperty("sampler.type").Get<string>();
			if (samplerName == "RANDOM") {
				config->Parse(
						Property("sampler.type")("SOBOL") <<
						Property("path.sampler.type")("SOBOL"));
			} else if (samplerName == "SOBOL") {
				config->Parse(
						Property("sampler.type")("METROPOLIS") <<
						Property("path.sampler.type")("METROPOLIS"));
			} else {
				config->Parse(
						Property("sampler.type")("RANDOM") <<
						Property("path.sampler.type")("RANDOM"));
			}
			session = new RenderSession(config);

			// Re-start the rendering
			session->Start();
			break;
		}
		case 'n': {
			const u_int screenRefreshInterval = config->GetProperty("screen.refresh.interval").Get<u_int>();
			if (screenRefreshInterval > 1000)
				config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(1000u, screenRefreshInterval - 1000))));
			else if (screenRefreshInterval > 100)
				config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(50u, screenRefreshInterval - 50))));
			else
				config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(10u, screenRefreshInterval - 5))));
			break;
		}
		case 'm': {
			const u_int screenRefreshInterval = config->GetProperty("screen.refresh.interval").Get<u_int>();
			if (screenRefreshInterval >= 1000)
				config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 1000)));
			else if (screenRefreshInterval >= 100)
				config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 50)));
			else
				config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 5)));
			break;
		}
//		case 't':
//			// Toggle tonemap type
//			if (session->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) {
//				Reinhard02ToneMapParams params;
//				session->film->SetToneMapParams(params);
//			} else {
//				LinearToneMapParams params;
//				session->film->SetToneMapParams(params);
//			}
			break;
		case 'v':
			optMoveScale = Max(.0125f, optMoveScale - ((optMoveScale>= 1.f) ? .25f : 0.0125f));
			UpdateMoveStep();
			SLG_LOG("Camera move scale: " << optMoveScale);
			break;
		case 'b':
			optMoveScale = Min(100.f, optMoveScale +  ((optMoveScale>= 1.f) ? .25f : 0.0125f));
			UpdateMoveStep();
			SLG_LOG("Camera move scale: " << optMoveScale);
			break;
//		case 'y': {
//			if (session->renderConfig->scene->camera->IsHorizontalStereoEnabled()) {
//				session->Stop();
//				session->renderConfig->scene->camera->SetOculusRiftBarrel(!session->renderConfig->scene->camera->IsOculusRiftBarrelEnabled());
//				session->renderConfig->scene->camera->Update(
//					session->GetFilm().GetWidth(), session->GetFilm().GetHeight());
//				session->Start();
//			}
//			break;
//		}
//		case 'u': {
//			session->Stop();
//			session->renderConfig->scene->camera->SetHorizontalStereo(!session->renderConfig->scene->camera->IsHorizontalStereoEnabled());
//			session->renderConfig->scene->camera->Update(
//				session->GetFilm().GetWidth(), session->GetFilm().GetHeight());
//			session->renderConfig->scene->editActions.AddAction(CAMERA_EDIT);
//			session->Start();
//			break;
//		}
//		case 'k': {
//			session->BeginSceneEdit();
//			const float currentEyeDistance = session->renderConfig->scene->camera->GetHorizontalStereoEyesDistance();
//			const float newEyeDistance = currentEyeDistance + ((currentEyeDistance == 0.f) ? .0626f : (currentEyeDistance * 0.05f));
//			SLG_LOG("Camera horizontal stereo eyes distance: " << newEyeDistance);
//			session->renderConfig->scene->camera->SetHorizontalStereoEyesDistance(newEyeDistance);
//			session->renderConfig->scene->camera->Update(
//				session->GetFilm().GetWidth(), session->GetFilm().GetHeight());
//			session->renderConfig->scene->editActions.AddAction(CAMERA_EDIT);
//			session->EndSceneEdit();
//			break;
//		}
//		case 'l': {
//			session->BeginSceneEdit();
//			const float currentEyeDistance = session->renderConfig->scene->camera->GetHorizontalStereoEyesDistance();
//			const float newEyeDistance = Max(0.f, currentEyeDistance - currentEyeDistance * 0.05f);
//			SLG_LOG("Camera horizontal stereo eyes distance: " << newEyeDistance);
//			session->renderConfig->scene->camera->SetHorizontalStereoEyesDistance(newEyeDistance);
//			session->renderConfig->scene->camera->Update(
//				session->GetFilm().GetWidth(), session->GetFilm().GetHeight());
//			session->renderConfig->scene->editActions.AddAction(CAMERA_EDIT);
//			session->EndSceneEdit();
//			break;
//		}
//		case ',': {
//			session->BeginSceneEdit();
//			const float currentLenDistance = session->renderConfig->scene->camera->GetHorizontalStereoLensDistance();
//			const float newLensDistance = currentLenDistance + ((currentLenDistance == 0.f) ? .1f : (currentLenDistance * 0.05f));
//			SLG_LOG("Camera horizontal stereo lens distance: " << newLensDistance);
//			session->renderConfig->scene->camera->SetHorizontalStereoLensDistance(newLensDistance);
//			session->renderConfig->scene->camera->Update(
//				session->GetFilm().GetWidth(), session->GetFilm().GetHeight());
//			session->renderConfig->scene->editActions.AddAction(CAMERA_EDIT);
//			session->EndSceneEdit();
//			break;
//		}
//		case '.': {
//			session->BeginSceneEdit();
//			const float currentLensDistance = session->renderConfig->scene->camera->GetHorizontalStereoLensDistance();
//			const float newLensDistance = Max(0.f, currentLensDistance - currentLensDistance * 0.05f);
//			SLG_LOG("Camera horizontal stereo lens distance: " << newLensDistance);
//			session->renderConfig->scene->camera->SetHorizontalStereoLensDistance(newLensDistance);
//			session->renderConfig->scene->camera->Update(
//				session->GetFilm().GetWidth(), session->GetFilm().GetHeight());
//			session->renderConfig->scene->editActions.AddAction(CAMERA_EDIT);
//			session->EndSceneEdit();
//			break;
//		}
		case '1':
			SetRenderingEngineType("PATHOCL");
			glutIdleFunc(NULL);
			glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '2':
			SetRenderingEngineType("LIGHTCPU");
			glutIdleFunc(NULL);
			glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '3':
			SetRenderingEngineType("PATHCPU");
			glutIdleFunc(NULL);
			glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '4':
			SetRenderingEngineType("BIDIRCPU");
			glutIdleFunc(NULL);
			glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '5':
			SetRenderingEngineType("BIDIRHYBRID");
			glutIdleFunc(NULL);
			glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '6':
			SetRenderingEngineType("CBIDIRHYBRID");
			glutIdleFunc(NULL);
			glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '7':
			SetRenderingEngineType("BIDIRVMCPU");
			glutIdleFunc(NULL);
			glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
			optRealTimeMode = false;
			break;
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case '8':
			SetRenderingEngineType("RTPATHOCL");
			glutIdleFunc(idleFunc);
			optRealTimeMode = true;
			if (config->GetProperty("screen.refresh.interval").Get<u_int>() > 25)
				config->Parse(Properties().Set(Property("screen.refresh.interval")(25)));
			break;
#endif
		case '9':
			SetRenderingEngineType("PATHHYBRID");
			glutIdleFunc(NULL);
			glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '0':
			SetRenderingEngineType("BIASPATHCPU");
			glutIdleFunc(NULL);
			glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '-':
			SetRenderingEngineType("BIASPATHOCL");
			glutIdleFunc(NULL);
			glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
			optRealTimeMode = false;
			break;
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case '=':
			SetRenderingEngineType("RTBIASPATHOCL");
			glutIdleFunc(idleFunc);
			optRealTimeMode = true;
			break;
#endif
		case 'o': {
#if defined(WIN32)
			std::wstring ws;
			if (optUseLuxVRName)
				ws.assign(LUXVR_LABEL.begin (), slg::LUXVR_LABEL.end());
			else
				ws.assign(SLG_LABEL.begin (), slg::SLG_LABEL.end());
			HWND hWnd = FindWindowW(NULL, ws.c_str());
			if (GetWindowLongPtr(hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST)
				SetWindowPos(hWnd, HWND_NOTOPMOST, NULL, NULL, NULL, NULL, SWP_NOMOVE | SWP_NOSIZE);
			else
				SetWindowPos(hWnd, HWND_TOPMOST, NULL, NULL, NULL, NULL, SWP_NOMOVE | SWP_NOSIZE);
#endif
			break;
		}
		default:
			break;
	}

	if (!optRealTimeMode)
		displayFunc();
}

void specialFunc(int key, int x, int y) {
	switch (key) {
		case GLUT_KEY_UP:
			session->BeginSceneEdit();
			config->GetScene().GetCamera().RotateUp(optRotateStep);
			session->EndSceneEdit();
			break;
		case GLUT_KEY_DOWN:
			session->BeginSceneEdit();
			config->GetScene().GetCamera().RotateDown(optRotateStep);
			session->EndSceneEdit();
			break;
		case GLUT_KEY_LEFT:
			session->BeginSceneEdit();
			config->GetScene().GetCamera().RotateLeft(optRotateStep);
			session->EndSceneEdit();
			break;
		case GLUT_KEY_RIGHT:
			session->BeginSceneEdit();
			config->GetScene().GetCamera().RotateRight(optRotateStep);
			session->EndSceneEdit();
			break;
		default:
			break;
	}

	if (!optRealTimeMode)
		displayFunc();
}

static int mouseButton0 = 0;
static int mouseButton2 = 0;
static int mouseGrabLastX = 0;
static int mouseGrabLastY = 0;
static double lastMouseUpdate = 0.0;

static void mouseFunc(int button, int state, int x, int y) {
	if (button == 0) {
		if (state == GLUT_DOWN) {
			// Record start position
			mouseGrabLastX = x;
			mouseGrabLastY = y;
			mouseButton0 = 1;
		} else if (state == GLUT_UP) {
			mouseButton0 = 0;
		}
	} else if (button == 2) {
		if (state == GLUT_DOWN) {
			// Record start position
			mouseGrabLastX = x;
			mouseGrabLastY = y;
			mouseButton2 = 1;
		} else if (state == GLUT_UP) {
			mouseButton2 = 0;
		}
	}
}

static void motionFunc(int x, int y) {
	const double minInterval = 0.2;

	if (mouseButton0) {
		// Check elapsed time since last update
		if (optRealTimeMode || (WallClockTime() - lastMouseUpdate > minInterval)) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			session->BeginSceneEdit();

			if (optMouseGrabMode) {
				config->GetScene().GetCamera().RotateUp(.04f * distY * optRotateStep);
				config->GetScene().GetCamera().RotateLeft(.04f * distX * optRotateStep);
			}
			else {
				config->GetScene().GetCamera().RotateDown(.04f * distY * optRotateStep);
				config->GetScene().GetCamera().RotateRight(.04f * distX * optRotateStep);
			};

			session->EndSceneEdit();

			mouseGrabLastX = x;
			mouseGrabLastY = y;

			if (!optRealTimeMode)
				displayFunc();
			lastMouseUpdate = WallClockTime();
		}
	} else if (mouseButton2) {
		// Check elapsed time since last update
		if (optRealTimeMode || (WallClockTime() - lastMouseUpdate > minInterval)) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			session->BeginSceneEdit();

			if (optMouseGrabMode) {
				config->GetScene().GetCamera().TranslateLeft(.04f * distX * optMoveStep);
				config->GetScene().GetCamera().TranslateForward(.04f * distY * optMoveStep);
			}
			else {
				config->GetScene().GetCamera().TranslateRight(.04f * distX * optMoveStep);
				config->GetScene().GetCamera().TranslateBackward(.04f * distY * optMoveStep);
			}

			session->EndSceneEdit();

			mouseGrabLastX = x;
			mouseGrabLastY = y;

			if (!optRealTimeMode)
				displayFunc();
			lastMouseUpdate = WallClockTime();
		}
	}
}

void InitGlut(int argc, char *argv[], const u_int width, const u_int height) {
	glutInit(&argc, argv);

	if (optUseGameMode) {
		const string screenDef = ToString(width) + "x" + ToString(height);
		glutGameModeString(screenDef.c_str());
		glutEnterGameMode();	
	} else {
		glutInitWindowSize(width, height);
		// Center the window
		const u_int scrWidth = glutGet(GLUT_SCREEN_WIDTH);
		const u_int scrHeight = glutGet(GLUT_SCREEN_HEIGHT);
		if ((scrWidth + 50 < width) || (scrHeight + 50 < height))
			glutInitWindowPosition(0, 0);
		else
			glutInitWindowPosition((scrWidth - width) / 2, (scrHeight - height) / 2);

		glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
		if (optUseLuxVRName)
			glutCreateWindow(slg::LUXVR_LABEL.c_str());
		else
			glutCreateWindow(slg::SLG_LABEL.c_str());
	}
}

void RunGlut() {
	glutReshapeFunc(reshapeFunc);
	glutKeyboardFunc(keyFunc);
	glutSpecialFunc(specialFunc);
	glutDisplayFunc(displayFunc);
	glutMouseFunc(mouseFunc);
	glutMotionFunc(motionFunc);
#if !defined(LUXRAYS_DISABLE_OPENCL)
	const string engineType = config->GetProperty("renderengine.type").Get<string>();
	if ((engineType == "RTPATHOCL") || (engineType == "RTBIASPATHOCL")) {
		glutIdleFunc(idleFunc);
		optRealTimeMode = true;
	} else
#endif
	{
		glutTimerFunc(config->GetProperty("screen.refresh.interval").Get<u_int>(), timerFunc, 0);
		optRealTimeMode = false;
	}

	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, session->GetFilm().GetWidth(), session->GetFilm().GetHeight());
	glLoadIdentity();
	glOrtho(0.f, session->GetFilm().GetWidth() - 1.f,
			0.f, session->GetFilm().GetHeight() - 1.f, -1.f, 1.f);

	glutMainLoop();
}
