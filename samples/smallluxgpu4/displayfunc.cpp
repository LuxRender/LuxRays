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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32)
#define _USE_MATH_DEFINES
#endif
#include <math.h>

// Jens's patch for MacOS
#if defined(__APPLE__)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "smalllux.h"
#include "displayfunc.h"
#include "renderconfig.h"
#include "rendersession.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/film/film.h"

bool OSDPrintHelp = false;

static char captionBuffer[512];

static void PrintString(void *font, const char *string) {
	int len, i;

	len = (int)strlen(string);
	for (i = 0; i < len; i++)
		glutBitmapCharacter(font, string[i]);
}

static void PrintHelpString(const unsigned int x, const unsigned int y, const char *key, const char *msg) {
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
	glRecti(10, 20, session->film->GetWidth() - 10, session->film->GetHeight() - 20);
	glDisable(GL_BLEND);

	glColor3f(1.f, 1.f, 1.f);
	int fontOffset = session->film->GetHeight() - 20 - 20;
	glRasterPos2i((session->film->GetWidth() - glutBitmapLength(GLUT_BITMAP_9_BY_15, (unsigned char *)"Help & Settings & Devices")) / 2, fontOffset);
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
	fontOffset -= 15;
#if defined(WIN32)
	PrintHelpString(15, fontOffset, "o", "windows always on top");
	fontOffset -= 15;
#endif

	// Settings
	char buf[512];
	glColor3f(0.5f, 1.0f, 0.f);
	fontOffset -= 15;
	glRasterPos2i(15, fontOffset);
	PrintString(GLUT_BITMAP_8_BY_13, "Settings:");
	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);
	sprintf(buf, "[Rendering time %dsecs][Screen refresh %dms]",
			int(session->renderEngine->GetRenderingTime()),
			session->renderConfig->GetScreenRefreshInterval());
	PrintString(GLUT_BITMAP_8_BY_13, buf);
	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);
	sprintf(buf, "[Render engine %s][Sampler %s][Tone mapping %s]",
			RenderEngine::RenderEngineType2String(session->renderEngine->GetEngineType()).c_str(),
			session->renderConfig->cfg.GetString("sampler.type", "RANDOM").c_str(),
			(session->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) ? "LINEAR" : "REINHARD02");
	PrintString(GLUT_BITMAP_8_BY_13, buf);
	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);

	// Intersection devices
	const vector<IntersectionDevice *> &idevices = session->renderEngine->GetIntersectionDevices();

	double minPerf = idevices[0]->GetTotalPerformance();
	double totalPerf = idevices[0]->GetTotalPerformance();
	for (size_t i = 1; i < idevices.size(); ++i) {
		minPerf = min(minPerf, idevices[i]->GetTotalPerformance());
		totalPerf += idevices[i]->GetTotalPerformance();
	}

	glColor3f(1.0f, 0.5f, 0.f);
	int offset = 45;
	size_t deviceCount = idevices.size();

	char buff[512];
	for (size_t i = 0; i < deviceCount; ++i) {
		sprintf(buff, "[%s][Rays/sec %dK (%dK + %dK)][Prf Idx %.2f][Wrkld %.1f%%][Mem %dM/%dM]",
			idevices[i]->GetName().c_str(),
			int(idevices[i]->GetTotalPerformance() / 1000.0),
				int(idevices[i]->GetSerialPerformance() / 1000.0),
				int(idevices[i]->GetDataParallelPerformance() / 1000.0),
			idevices[i]->GetTotalPerformance() / minPerf,
			100.0 * idevices[i]->GetTotalPerformance() / totalPerf,
			int(idevices[i]->GetUsedMemory() / (1024 * 1024)),
			int(idevices[i]->GetMaxMemory() / (1024 * 1024)));
		glRasterPos2i(20, offset);
		PrintString(GLUT_BITMAP_8_BY_13, buff);
		offset += 15;
	}

	glRasterPos2i(15, offset);
	PrintString(GLUT_BITMAP_9_BY_15, "Rendering devices:");
}

static void PrintCaptions() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.8f);
	glRecti(0, session->film->GetHeight() - 15,
			session->film->GetWidth() - 1, session->film->GetHeight() - 1);
	glRecti(0, 0, session->film->GetWidth() - 1, 18);
	glDisable(GL_BLEND);

	// Caption line 0
	glColor3f(1.f, 1.f, 1.f);
	glRasterPos2i(4, 5);
	PrintString(GLUT_BITMAP_8_BY_13, captionBuffer);

	// Title
	glRasterPos2i(4, session->film->GetHeight() - 10);
	PrintString(GLUT_BITMAP_8_BY_13, SLG_LABEL.c_str());
}

void displayFunc(void) {
	session->film->UpdateScreenBuffer();
	const float *pixels = session->film->GetScreenBuffer();

	glRasterPos2i(0, 0);
	glDrawPixels(session->film->GetWidth(), session->film->GetHeight(), GL_RGB, GL_FLOAT, pixels);

	PrintCaptions();

	if (OSDPrintHelp) {
		glPushMatrix();
		glLoadIdentity();
		glOrtho(-0.5f, session->film->GetWidth() - 0.5f,
				-0.5f, session->film->GetHeight() - 0.5f, -1.f, 1.f);

		PrintHelpAndSettings();

		glPopMatrix();
	}

	glutSwapBuffers();
}

void reshapeFunc(int newWidth, int newHeight) {
	// Check if width or height have really changed
	if ((newWidth != (int)session->film->GetWidth()) ||
			(newHeight != (int)session->film->GetHeight())) {
		glViewport(0, 0, newWidth, newHeight);
		glLoadIdentity();
		glOrtho(0.f, newWidth - 1.0f, 0.f, newHeight - 1.0f, -1.f, 1.f);

		session->BeginEdit();

		session->renderConfig->scene->camera->Update(newWidth, newHeight);
		session->editActions.AddAction(CAMERA_EDIT);

		session->film->Init(session->renderConfig->scene->camera->GetFilmWeight(),
			session->renderConfig->scene->camera->GetFilmHeight());
		session->editActions.AddAction(FILM_EDIT);

		session->EndEdit();

		glutPostRedisplay();
	}
}

void timerFunc(int value) {
	sprintf(captionBuffer, "[Pass %3d][Avg. samples/sec % 3.2fM][Avg. rays/sec % 4dK on %.1fK tris]",
		session->renderEngine->GetPass(),
		session->renderEngine->GetTotalSamplesSec() / 1000000.0,
		int(session->renderEngine->GetTotalRaysSec() / 1000.0),
		session->renderConfig->scene->dataSet->GetTotalTriangleCount() / 1000.f);

	// Need to update the Film
	session->renderEngine->UpdateFilm();

	// Check if periodic save is enabled
	if (session->NeedPeriodicSave()) {
		// Time to save the image and film
		session->SaveFilmImage();
	}

	glutPostRedisplay();

	glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
}

#define MOVE_STEP 0.5f
#define ROTATE_STEP 4.f
void keyFunc(unsigned char key, int x, int y) {
	switch (key) {
		case 'p': {
			session->SaveFilmImage();
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
			session->BeginEdit();
			session->renderConfig->scene->camera->TranslateLeft(MOVE_STEP);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();
			break;
		}
		case 'd': {
			session->BeginEdit();
			session->renderConfig->scene->camera->TranslateRight(MOVE_STEP);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();
			break;
		}
		case 'w': {
			session->BeginEdit();
			session->renderConfig->scene->camera->TranslateForward(MOVE_STEP);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();
			break;
		}
		case 's': {
			session->BeginEdit();
			session->renderConfig->scene->camera->TranslateBackward(MOVE_STEP);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();
			break;
		}
		case 'r':
			session->BeginEdit();
			session->renderConfig->scene->camera->Translate(Vector(0.f, 0.f, MOVE_STEP));
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();
			break;
		case 'f':
			session->BeginEdit();
			session->renderConfig->scene->camera->Translate(Vector(0.f, 0.f, -MOVE_STEP));
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();
			break;
		case 'h':
			OSDPrintHelp = (!OSDPrintHelp);
			break;
		case 'i':
			session->Stop();
			if (session->renderConfig->cfg.GetString("sampler.type", "RANDOM") == "RANDOM") {
				session->renderConfig->cfg.SetString("sampler.type", "METROPOLIS");
				session->renderConfig->cfg.SetString("path.sampler.type", "METROPOLIS");
			} else {
				session->renderConfig->cfg.SetString("sampler.type", "RANDOM");
				session->renderConfig->cfg.SetString("path.sampler.type", "RANDOM");				
			}
			session->Start();
			break;
		case 'n': {
			const unsigned int screenRefreshInterval = session->renderConfig->GetScreenRefreshInterval();
			if (screenRefreshInterval > 1000)
				session->renderConfig->SetScreenRefreshInterval(max(1000u, screenRefreshInterval - 1000));
			else
				session->renderConfig->SetScreenRefreshInterval(max(50u, screenRefreshInterval - 50));
			break;
		}
		case 'm': {
			const unsigned int screenRefreshInterval = session->renderConfig->GetScreenRefreshInterval();
			if (screenRefreshInterval >= 1000)
				session->renderConfig->SetScreenRefreshInterval(screenRefreshInterval + 1000);
			else
				session->renderConfig->SetScreenRefreshInterval(screenRefreshInterval + 50);
			break;
		}
		case 't':
			// Toggle tonemap type
			if (session->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) {
				Reinhard02ToneMapParams params;
				session->film->SetToneMapParams(params);
			} else {
				LinearToneMapParams params;
				session->film->SetToneMapParams(params);
			}
			break;
		case '1':
			session->SetRenderingEngineType(PATHOCL);
			break;
		case '2':
			session->SetRenderingEngineType(LIGHTCPU);
			break;
		case '3':
			session->SetRenderingEngineType(PATHCPU);
			break;
		case '4':
			session->SetRenderingEngineType(BIDIRCPU);
			break;
		case '5':
			session->SetRenderingEngineType(BIDIRHYBRID);
			break;
		case '6':
			session->SetRenderingEngineType(CBIDIRHYBRID);
			break;
		case '7':
			session->SetRenderingEngineType(BIDIRVMCPU);
			break;
		case 'o': {
#if defined(WIN32)
			std::wstring ws;
			ws.assign(SLG_LABEL.begin (), SLG_LABEL.end());
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

	displayFunc();
}

void specialFunc(int key, int x, int y) {
	switch (key) {
		case GLUT_KEY_UP:
			session->BeginEdit();
			session->renderConfig->scene->camera->RotateUp(ROTATE_STEP);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();
			break;
		case GLUT_KEY_DOWN:
			session->BeginEdit();
			session->renderConfig->scene->camera->RotateDown(ROTATE_STEP);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();
			break;
		case GLUT_KEY_LEFT:
			session->BeginEdit();
			session->renderConfig->scene->camera->RotateLeft(ROTATE_STEP);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();
			break;
		case GLUT_KEY_RIGHT:
			session->BeginEdit();
			session->renderConfig->scene->camera->RotateRight(ROTATE_STEP);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();
			break;
		default:
			break;
	}

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
		if (WallClockTime() - lastMouseUpdate > minInterval) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			session->BeginEdit();

			if (mouseGrabMode) {
				session->renderConfig->scene->camera->RotateUp(0.04f * distY * ROTATE_STEP);
				session->renderConfig->scene->camera->RotateLeft(0.04f * distX * ROTATE_STEP);
			}
			else {
				session->renderConfig->scene->camera->RotateDown(0.04f * distY * ROTATE_STEP);
				session->renderConfig->scene->camera->RotateRight(0.04f * distX * ROTATE_STEP);
			};

			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();

			mouseGrabLastX = x;
			mouseGrabLastY = y;

			displayFunc();
			lastMouseUpdate = WallClockTime();
		}
	} else if (mouseButton2) {
		// Check elapsed time since last update
		if (WallClockTime() - lastMouseUpdate > minInterval) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			session->BeginEdit();

			if (mouseGrabMode) {
				session->renderConfig->scene->camera->TranslateLeft(0.04f * distX * MOVE_STEP);
				session->renderConfig->scene->camera->TranslateForward(0.04f * distY * MOVE_STEP);
			}
			else {
				session->renderConfig->scene->camera->TranslateRight(0.04f * distX * MOVE_STEP);
				session->renderConfig->scene->camera->TranslateBackward(0.04f * distY * MOVE_STEP);				
			}

			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndEdit();

			mouseGrabLastX = x;
			mouseGrabLastY = y;

			displayFunc();
			lastMouseUpdate = WallClockTime();
		}
	}
}

void InitGlut(int argc, char *argv[], const unsigned int width, const unsigned int height) {
	glutInit(&argc, argv);

	glutInitWindowSize(width, height);
	// Center the window
	unsigned int scrWidth = glutGet(GLUT_SCREEN_WIDTH);
	unsigned int scrHeight = glutGet(GLUT_SCREEN_HEIGHT);
	if ((scrWidth + 50 < width) || (scrHeight + 50 < height))
		glutInitWindowPosition(0, 0);
	else
		glutInitWindowPosition((scrWidth - width) / 2, (scrHeight - height) / 2);

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutCreateWindow(SLG_LABEL.c_str());
}

void RunGlut() {
	glutReshapeFunc(reshapeFunc);
	glutKeyboardFunc(keyFunc);
	glutSpecialFunc(specialFunc);
	glutDisplayFunc(displayFunc);
	glutMouseFunc(mouseFunc);
	glutMotionFunc(motionFunc);
	glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);

	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, session->film->GetWidth(), session->film->GetHeight());
	glLoadIdentity();
	glOrtho(0.f, session->film->GetWidth() - 1.f,
			0.f, session->film->GetHeight() - 1.f, -1.f, 1.f);

	glutMainLoop();
}
