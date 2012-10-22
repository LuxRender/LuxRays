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

#ifndef LIGHTCPU_H
#define	LIGHTCPU_H

#include "smalllux.h"
#include "renderengine.h"

#include <boost/thread/thread.hpp>

class LightCPURenderEngine;

//------------------------------------------------------------------------------
// Light tracing CPU-only render threads
//------------------------------------------------------------------------------

class LightCPURenderThread {
public:
	LightCPURenderThread(const unsigned int index, const unsigned int seedBase,
			LightCPURenderEngine *re);
	~LightCPURenderThread();

	void Start();
    void Interrupt();
	void Stop();

	void BeginEdit();
	void EndEdit(const EditActionList &editActions);

	friend class LightCPURenderEngine;

private:
	static void RenderThreadImpl(LightCPURenderThread *renderThread);

	void StartRenderThread();
	void StopRenderThread();

	void InitRender();
	void SplatSample(const float scrX, const float scrY, const Spectrum &radiance);

	unsigned int threadIndex;
	unsigned int seed;
	LightCPURenderEngine *renderEngine;
	SampleBuffer *sampleBuffer;

	boost::thread *renderThread;
	NativeFilm *threadFilm;

	bool started, editMode, reportedPermissionError;
};

//------------------------------------------------------------------------------
// Light tracing CPU render engine
//------------------------------------------------------------------------------

class LightCPURenderEngine : public RenderEngine {
public:
	LightCPURenderEngine(RenderConfig *cfg, NativeFilm *flm, boost::mutex *flmMutex);
	virtual ~LightCPURenderEngine();

	void Start() {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		StartLockLess();
	}
	
	void Stop() {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		StopLockLess();
	}

	void BeginEdit() {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		BeginEditLockLess();
	}

	void EndEdit(const EditActionList &editActions) {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		EndEditLockLess(editActions);
	}

	void UpdateFilm();

	unsigned int GetPass() const;
	float GetConvergence() const;
	RenderEngineType GetEngineType() const { return LIGHTCPU; }
	double GetTotalSamplesSec() const {
		return (elapsedTime == 0.0) ? 0.0 : (samplesCount / elapsedTime);
	}
	double GetRenderingTime() const { return elapsedTime; }

	friend class LightCPURenderThread;

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrImportanceCap;

	float convergence;
	double lastConvergenceTestTime;
	unsigned long long lastConvergenceTestSamplesCount;

private:
	void StartLockLess();
	void StopLockLess();

	void BeginEditLockLess();
	void EndEditLockLess(const EditActionList &editActions);

	void UpdateFilmLockLess();

	mutable boost::mutex engineMutex;

	vector<LightCPURenderThread *> renderThreads;

	double startTime;
	double elapsedTime;
	unsigned long long samplesCount;

	Context *ctx;
};

#endif	/* LIGHTCPU_H */
