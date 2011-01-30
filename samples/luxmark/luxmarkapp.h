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

#ifndef _LUXMARKAPP_H
#define _LUXMARKAPP_H

#include <QtGui/QApplication>
#include <QTimer>

#include <boost/thread.hpp>

#include "mainwindow.h"
#include "renderconfig.h"

enum LuxMarkAppMode {
	BENCHMARK, INTERACTIVE
};

// List of supported scenes
#define SCENE_LUXBALL_HDR "scenes/luxball/render-hdr.cfg"
#define SCENE_LUXBALL "scenes/luxball/render.cfg"

class LuxMarkApp : public QApplication {
	Q_OBJECT

public:
	MainWindow *mainWin;

	LuxMarkApp(int argc, char **argv);
	~LuxMarkApp();
	
	void Init(void);

	void SetMode(LuxMarkAppMode m);
	void SetScene(const char *name);

private:
	void InitRendering(LuxMarkAppMode m, const char *scnName);

	LuxMarkAppMode mode;
	const char *sceneName;

	boost::thread *engineThread;
	RenderingConfig *renderConfig;

	QTimer *renderRefreshTimer;

private slots:
	void RenderRefreshTimeout();
};

#endif // _LUXMARKAPP_H
