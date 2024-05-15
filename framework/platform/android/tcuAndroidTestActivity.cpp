/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Android test activity.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidTestActivity.hpp"
#include "tcuAndroidUtil.hpp"

#include <android/window.h>

#include <chrono>
#include <string>
#include <stdlib.h>

extern long long g_accumulatedMillisecondsDraw ;
extern long long g_accumulatedMillisecondsVerify ;
extern long long g_accumulatedMillisecondsMakeTests ;
extern long long g_accumulatedMillisecondsConstruct ;
extern long long g_accumulatedMillisecondsConstruct1 ;
extern long long g_accumulatedMillisecondsConstruct2 ;
extern long long g_accumulatedMillisecondsConstruct3 ;
extern long long g_accumulatedMillisecondsConstruct4 ;
extern long long g_accumulatedMillisecondsConstruct5 ;
extern long long g_accumulatedMillisecondsConstruct6 ;
extern long long g_accumulatedMillisecondsConstruct8 ;
extern long long g_accumulatedMillisecondsConstruct9 ;

using std::string;

namespace tcu
{
namespace Android
{

// TestThread

TestThread::TestThread (NativeActivity& activity, const std::string& cmdLineString, const CommandLine& cmdLine)
	: RenderThread	(activity)
	, m_cmdLine		(cmdLine)
	, m_platform	(activity)
	, m_archive		(activity.getNativeActivity()->assetManager)
	, m_log			(m_cmdLine.getLogFileName(), m_cmdLine.getLogFlags())
	, m_app			(m_platform, m_archive, m_log, m_cmdLine)
	, m_finished	(false)
{
	const std::string sessionInfo = "#sessionInfo commandLineParameters \"";
	m_log.writeSessionInfo(sessionInfo + cmdLineString + "\"\n");
}

TestThread::~TestThread (void)
{
	// \note m_testApp is managed by thread.
}

void TestThread::run (void)
{
	RenderThread::run();
}

void TestThread::onWindowCreated (ANativeWindow* window)
{
	m_platform.getWindowRegistry().addWindow(window);
}

void TestThread::onWindowDestroyed (ANativeWindow* window)
{
	m_platform.getWindowRegistry().destroyWindow(window);
}

void TestThread::onWindowResized (ANativeWindow* window)
{
	DE_UNREF(window);
	print("Warning: Native window was resized, results may be undefined");
}

bool TestThread::render (void)
{
	if (!m_finished)
		m_finished = !m_app.iterate();
	return !m_finished;
}

// TestActivity

TestActivity::TestActivity (ANativeActivity* activity)
	: RenderActivity	(activity)
	, m_cmdLine			(getIntentStringExtra(activity, "cmdLine"))
	, m_testThread		(*this, getIntentStringExtra(activity, "cmdLine"), m_cmdLine)
	, m_started			(false)
{
	// Set initial orientation.
	setRequestedOrientation(getNativeActivity(), mapScreenRotation(m_cmdLine.getScreenRotation()));

	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::seconds;
	m_timestart = high_resolution_clock::now();


	// Set up window flags.
	ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON	|
											 AWINDOW_FLAG_TURN_SCREEN_ON	|
											 AWINDOW_FLAG_FULLSCREEN		|
											 AWINDOW_FLAG_SHOW_WHEN_LOCKED, 0);
}

TestActivity::~TestActivity (void)
{
}

void TestActivity::onStart (void)
{
	if (!m_started)
	{
		setThread(&m_testThread);
		m_testThread.start();
		m_started = true;
	}

	RenderActivity::onStart();
}

void TestActivity::onDestroy (void)
{
	if (m_started)
	{
		setThread(DE_NULL);
		m_testThread.stop();
		m_started = false;
	}

	RenderActivity::onDestroy();

	auto t2 = std::chrono::high_resolution_clock::now();
	auto ms_int = std::chrono::duration_cast<std::chrono::seconds>(t2 - m_timestart);

	tcu::print("Tom: finished in %lld seconds g_accumulatedMillisecondsDraw: %lld g_accumulatedMillisecondsVerify: %lld , g_accumulatedMillisecondsMakeTests %lld, g_accumulatedMillisecondsConstruct: %lld\n",
	 ms_int.count(), g_accumulatedMillisecondsDraw, g_accumulatedMillisecondsVerify, g_accumulatedMillisecondsMakeTests, g_accumulatedMillisecondsConstruct);


	tcu::print("Tom: finished in g_accumulatedMillisecondsConstruct1 %lld seconds \n", g_accumulatedMillisecondsConstruct1);
	tcu::print("Tom: finished in g_accumulatedMillisecondsConstruct2 %lld seconds \n", g_accumulatedMillisecondsConstruct2);
	tcu::print("Tom: finished in g_accumulatedMillisecondsConstruct3 %lld seconds \n", g_accumulatedMillisecondsConstruct3);
	tcu::print("Tom: finished in g_accumulatedMillisecondsConstruct4 %lld seconds \n", g_accumulatedMillisecondsConstruct4);
	tcu::print("Tom: finished in g_accumulatedMillisecondsConstruct5 %lld seconds \n", g_accumulatedMillisecondsConstruct5);
	tcu::print("Tom: finished in g_accumulatedMillisecondsConstruct6 %lld seconds \n", g_accumulatedMillisecondsConstruct6);
//	tcu::print("Tom: finished in g_accumulatedMillisecondsConstruct7 %lld seconds \n", g_accumulatedMillisecondsConstruct7);
	tcu::print("Tom: finished in g_accumulatedMillisecondsConstruct8 %lld seconds \n", g_accumulatedMillisecondsConstruct8);
	tcu::print("Tom: finished in g_accumulatedMillisecondsConstruct9 %lld seconds \n", g_accumulatedMillisecondsConstruct9);
	// Kill this process.
	print("Done, killing process");
	exit(0);
}

void TestActivity::onConfigurationChanged (void)
{
	RenderActivity::onConfigurationChanged();

	// Update rotation.
	setRequestedOrientation(getNativeActivity(), mapScreenRotation(m_cmdLine.getScreenRotation()));
}

} // Android
} // tcu
