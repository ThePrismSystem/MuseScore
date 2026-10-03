/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include <cmath>

#include "midirecording/internal/takerecorder.h"

using namespace mu::midirecording;

class MidiRecording_TakeRecorderTests : public ::testing::Test
{
};

static int64_t takeRecorderTestNs(double secs)
{
    return static_cast<int64_t>(std::llround(secs * 1e9));
}

static ClockSample takeRecorderTestSample(double hostSecs, double playbackSecs)
{
    return { takeRecorderTestNs(hostSecs), playbackSecs };
}

//! Starts a take and runs playback through its first forward step, ending on
//! the report (0.02 s, 0.01 s)
static void takeRecorderTestStartRecording(TakeRecorder& recorder)
{
    recorder.start(TakeFile());
    recorder.addSample(takeRecorderTestSample(0.0, 0.0));
    recorder.addSample(takeRecorderTestSample(0.01, 0.0));
    ASSERT_EQ(recorder.addSample(takeRecorderTestSample(0.02, 0.01)), TakeRecorder::SampleResult::RecordingStarted);
}

TEST_F(MidiRecording_TakeRecorderTests, IdleIgnoresEverything)
{
    TakeRecorder recorder;

    recorder.addEvent({ 1, true, 60, 100 });
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.0, 0.0)), TakeRecorder::SampleResult::Ignored);
    EXPECT_FALSE(recorder.isActive());
    EXPECT_EQ(recorder.state(), TakeRecorder::State::Idle);
    EXPECT_FALSE(recorder.stop(10).has_value());
}

TEST_F(MidiRecording_TakeRecorderTests, StartKeepsTheContextAndClearsTheCapture)
{
    TakeFile context;
    context.startTick = 960;
    context.staffIdx = 2;
    context.voice = 1;
    context.replaceMode = "voice";
    context.countInBars = 2;
    context.settings.gridTicks = 240;
    context.stopNs = 5;
    context.events = { { 1, true, 60, 100 } };
    context.clock = { { 1, 0.5 } };

    TakeRecorder recorder;
    recorder.start(context);
    EXPECT_TRUE(recorder.isActive());
    EXPECT_EQ(recorder.state(), TakeRecorder::State::CountIn);

    const std::optional<TakeFile> take = recorder.stop(takeRecorderTestNs(1.0));

    ASSERT_TRUE(take.has_value());
    EXPECT_EQ(take->startTick, 960);
    EXPECT_EQ(take->staffIdx, 2);
    EXPECT_EQ(take->voice, 1);
    EXPECT_EQ(take->replaceMode, "voice");
    EXPECT_EQ(take->countInBars, 2);
    EXPECT_EQ(take->settings.gridTicks, 240);
    EXPECT_TRUE(take->events.empty());
    EXPECT_TRUE(take->clock.empty());
    EXPECT_EQ(take->stopNs, takeRecorderTestNs(1.0));
    EXPECT_FALSE(recorder.isActive());
}

TEST_F(MidiRecording_TakeRecorderTests, CountInEndsAtTheFirstForwardStep)
{
    TakeRecorder recorder;
    recorder.start(TakeFile());

    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.0, 0.0)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.5, 0.0)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.state(), TakeRecorder::State::CountIn);

    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.51, 0.01)), TakeRecorder::SampleResult::RecordingStarted);
    EXPECT_EQ(recorder.state(), TakeRecorder::State::Recording);
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.52, 0.02)), TakeRecorder::SampleResult::Kept);

    const std::optional<TakeFile> take = recorder.stop(takeRecorderTestNs(1.0));
    ASSERT_TRUE(take.has_value());
    EXPECT_EQ(take->clock.size(), 4u);
}

//! Starting a take seeks playback to the start, and the report of that seek
//! (a jump from wherever playback stood) arrives during the count-in
TEST_F(MidiRecording_TakeRecorderTests, SeekBeforePlaybackMovesDoesNotEndTheTake)
{
    TakeRecorder recorder;
    recorder.start(TakeFile());

    // Playback stood at 5 s; the take starts at 2 s
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.0, 5.0)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.001, 2.0)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.5, 2.0)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.state(), TakeRecorder::State::CountIn);
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.51, 2.01)), TakeRecorder::SampleResult::RecordingStarted);

    // Playback stood at 0 s; the take starts at 38 s
    recorder.start(TakeFile());
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(1.0, 0.0)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(1.001, 38.0)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.state(), TakeRecorder::State::CountIn);
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(1.5, 38.0)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(1.51, 38.01)), TakeRecorder::SampleResult::RecordingStarted);
}

TEST_F(MidiRecording_TakeRecorderTests, JumpWhileRecordingEndsTheTakeThere)
{
    TakeRecorder recorder;
    takeRecorderTestStartRecording(recorder);
    recorder.addEvent({ takeRecorderTestNs(0.5), true, 60, 100 });
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(1.0, 0.99)), TakeRecorder::SampleResult::Kept);

    // Playback stops at the end of the score and rewinds to 0
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(1.01, 0.0)), TakeRecorder::SampleResult::PlaybackMoved);
    EXPECT_EQ(recorder.state(), TakeRecorder::State::Ended);
    EXPECT_TRUE(recorder.isActive());

    recorder.addEvent({ takeRecorderTestNs(1.2), false, 60, 0 });
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(1.3, 0.01)), TakeRecorder::SampleResult::Ignored);

    const std::optional<TakeFile> take = recorder.stop(takeRecorderTestNs(2.0));

    ASSERT_TRUE(take.has_value());
    EXPECT_EQ(take->stopNs, takeRecorderTestNs(1.01));
    EXPECT_EQ(take->events.size(), 1u);
    EXPECT_DOUBLE_EQ(take->clock.back().playbackSecs, 0.99);
    EXPECT_FALSE(recorder.isActive());
}

TEST_F(MidiRecording_TakeRecorderTests, ForwardSeekWhileRecordingEndsTheTake)
{
    TakeRecorder recorder;
    takeRecorderTestStartRecording(recorder);

    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.03, 4.0)), TakeRecorder::SampleResult::PlaybackMoved);

    const std::optional<TakeFile> take = recorder.stop(takeRecorderTestNs(1.0));
    ASSERT_TRUE(take.has_value());
    EXPECT_EQ(take->stopNs, takeRecorderTestNs(0.03));
}

TEST_F(MidiRecording_TakeRecorderTests, BunchedReportsAreNotASeek)
{
    TakeRecorder recorder;
    takeRecorderTestStartRecording(recorder);

    // Three reports the queue delivered within 0.2 ms, 20 ms of playback apart
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.1, 0.03)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.1001, 0.05)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.1002, 0.07)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.state(), TakeRecorder::State::Recording);
}

TEST_F(MidiRecording_TakeRecorderTests, RepeatedReportWhileRecordingIsKept)
{
    TakeRecorder recorder;
    takeRecorderTestStartRecording(recorder);

    EXPECT_EQ(recorder.addSample(takeRecorderTestSample(0.03, 0.01)), TakeRecorder::SampleResult::Kept);
    EXPECT_EQ(recorder.state(), TakeRecorder::State::Recording);
}

TEST_F(MidiRecording_TakeRecorderTests, StopDuringCountInReturnsWhatWasPlayed)
{
    TakeRecorder recorder;
    recorder.start(TakeFile());
    recorder.addSample(takeRecorderTestSample(0.0, 0.0));
    recorder.addEvent({ takeRecorderTestNs(0.4), true, 60, 90 });

    const std::optional<TakeFile> take = recorder.stop(takeRecorderTestNs(0.5));

    ASSERT_TRUE(take.has_value());
    EXPECT_EQ(take->events.size(), 1u);
    EXPECT_EQ(take->clock.size(), 1u);
    EXPECT_EQ(take->stopNs, takeRecorderTestNs(0.5));
}

TEST_F(MidiRecording_TakeRecorderTests, CancelDropsTheTake)
{
    TakeRecorder recorder;
    recorder.start(TakeFile());
    recorder.addEvent({ 1, true, 60, 90 });

    recorder.cancel();

    EXPECT_FALSE(recorder.isActive());
    EXPECT_FALSE(recorder.stop(10).has_value());

    recorder.start(TakeFile());
    const std::optional<TakeFile> take = recorder.stop(10);
    ASSERT_TRUE(take.has_value());
    EXPECT_TRUE(take->events.empty());
}
