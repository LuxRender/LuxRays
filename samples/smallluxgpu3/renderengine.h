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

#ifndef _RENDERENGINE_H
#define	_RENDERENGINE_H

#include "smalllux.h"
#include "renderconfig.h"
#include "editaction.h"

#include "luxrays/utils/film/film.h"

enum RenderEngineType {
	PATHOCL  = 4,
	LIGHTCPU = 5,
	PATHCPU = 6
};

//------------------------------------------------------------------------------
// Base class for render engines
//------------------------------------------------------------------------------

class RenderEngine {
public:
	RenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~RenderEngine();

	void Start();
	void Stop();
	void BeginEdit();
	void EndEdit(const EditActionList &editActions);

	void UpdateFilm();

	virtual RenderEngineType GetEngineType() const = 0;

	unsigned int GetPass() const {
		return samplesCount / (film->GetWidth() * film->GetHeight());
	}
	float GetConvergence() const { return convergence; }
	double GetTotalSamplesSec() const {
		return (elapsedTime == 0.0) ? 0.0 : (samplesCount / elapsedTime);
	}
	double GetRenderingTime() const { return elapsedTime; }

	static RenderEngine *AllocRenderEngine(const RenderEngineType engineType,
		RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex);
	
protected:
	virtual void StartLockLess() = 0;
	virtual void StopLockLess() = 0;

	virtual void BeginEditLockLess() = 0;
	virtual void EndEditLockLess(const EditActionList &editActions) = 0;

	virtual void UpdateFilmLockLess() = 0;

	boost::mutex engineMutex;
	Context *ctx;

	RenderConfig *renderConfig;
	Film *film;
	boost::mutex *filmMutex;

	double startTime, elapsedTime;
	unsigned long long samplesCount;

	float convergence;
	double lastConvergenceTestTime;
	unsigned long long lastConvergenceTestSamplesCount;

	bool started, editMode;
};

//------------------------------------------------------------------------------
// Base class for OpenCL render engines
//------------------------------------------------------------------------------

class OCLRenderEngine : public RenderEngine {
public:
	OCLRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	const vector<OpenCLIntersectionDevice *> &GetIntersectionDevices() const {
		return oclIntersectionDevices;
	}

protected:
	vector<OpenCLIntersectionDevice *> oclIntersectionDevices;
};

//------------------------------------------------------------------------------
// Base class for CPU render engines
//------------------------------------------------------------------------------

class CPURenderEngine;

class CPURenderThread {
public:
	CPURenderThread(CPURenderEngine *engine, const unsigned int index,
			const unsigned int seedVal, void (* threadFunc)(CPURenderThread *),
			const bool perPixelNormalizationFilm, const bool perScreenNormalizationFilm);
	~CPURenderThread();

	void Start();
	void Interrupt();
	void Stop();

	void BeginEdit();
	void EndEdit(const EditActionList &editActions);

	void StartRenderThread();
	void StopRenderThread();

	void InitRender();
	void SplatSample(const float scrX, const float scrY, const Spectrum &radiance);

	friend class CPURenderEngine;

	// NOTE: all the fields are public so they can be accessed by renderThreadFunc()

	unsigned int threadIndex;
	unsigned int seed;
	void (* renderThreadFunc)(CPURenderThread *);
	CPURenderEngine *renderEngine;

	boost::thread *renderThread;
	Film *threadFilmPPN; // Per-Pixel-Normalization Film
	Film *threadFilmPSN; // Per-Screen-Normalization Film

	bool started, editMode;
	bool usePerPixelNormalizationFilm, usePerScreenNormalizationFilm;
};

class CPURenderEngine : public RenderEngine {
public:
	CPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex,
			void (* threadFunc)(CPURenderThread *),
			const bool perPixelNormalizationFilm, const bool perScreenNormalizationFilm);
	~CPURenderEngine();

	friend class CPURenderThread;

protected:
	void StartLockLess();
	void StopLockLess();

	void BeginEditLockLess();
	void EndEditLockLess(const EditActionList &editActions);

	void UpdateFilmLockLess();

	vector<CPURenderThread *> renderThreads;
	void (* renderThreadFunc)(CPURenderThread *renderThread);
};

#endif	/* _RENDERENGINE_H */
