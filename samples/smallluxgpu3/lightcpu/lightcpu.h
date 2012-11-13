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

#ifndef _LIGHTCPU_H
#define	_LIGHTCPU_H

#include "smalllux.h"
#include "renderengine.h"

//------------------------------------------------------------------------------
// Light tracing CPU render engine
//------------------------------------------------------------------------------

class LightCPURenderEngine;

class LightCPURenderThread : public CPURenderThread {
public:
	LightCPURenderThread(CPURenderEngine *engine, const u_int index, const u_int seedVal) :
		CPURenderThread(engine, index, seedVal, true, true), samplesCount(0.0) { }

	friend class LightCPURenderEngine;

private:
	boost::thread *AllocRenderThread() { return new boost::thread(&LightCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void ConnectToEye(const float u0,
			const BSDF &bsdf, const Point &lensPoint, const Spectrum &flux,
			vector<SampleResult> &sampleResults);

	double samplesCount;
};

class LightCPURenderEngine : public CPURenderEngine {
public:
	LightCPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return LIGHTCPU; }

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class LightCPURenderThread;

private:
	CPURenderThread *NewRenderThread(CPURenderEngine *engine,
		const u_int index, const unsigned int seedVal) {
		return new LightCPURenderThread(engine, index, seedVal);
	}

	void UpdateSamplesCount() {
		double count = 0.0;
		for (size_t i = 0; i < renderThreads.size(); ++i) {
			LightCPURenderThread *thread = (LightCPURenderThread *)renderThreads[i];
			if (thread)
				count += thread->samplesCount;
		}
		samplesCount = count;
	}
};

#endif	/* _LIGHTCPU_H */
