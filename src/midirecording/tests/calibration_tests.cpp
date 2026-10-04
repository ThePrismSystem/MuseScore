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

#include "midirecording/internal/calibration.h"
#include "midirecording/internal/timemap.h"

using namespace mu::midirecording;

class MidiRecording_CalibrationTests : public ::testing::Test
{
};

static int64_t calibrationTestNs(double secs)
{
    return static_cast<int64_t>(std::llround(secs * 1e9));
}

//! 120 bpm: 960 ticks per second of playback
static double calibrationTestTickToSecs(int tick)
{
    return tick / 960.0;
}

//! A calibration take from tick 0 over three 4/4 measures at 120 bpm.
//! Playback holds at 0 through a two-second count-in, then runs at speed
//! (seconds of playback per second of real time) for ten seconds.
static TakeFile calibrationTestTake(double speed = 1.0)
{
    TakeFile take;
    for (int i = 0; i < 200; ++i) {
        take.clock.push_back({ calibrationTestNs(i * 0.01), 0.0 });
    }
    for (int i = 0; i <= 1000; ++i) {
        const double t = 2.0 + i * 0.01;
        take.clock.push_back({ calibrationTestNs(t), speed * (t - 2.0) });
    }
    take.measures = { { 0, 1920, 4, 4 }, { 1920, 1920, 4, 4 }, { 3840, 1920, 4, 4 } };
    take.timeMap = buildTimeMap(0, 5760, 120, calibrationTestTickToSecs);
    return take;
}

//! A note played lateMs of real time after playback reaches tick
static void calibrationTestPlay(TakeFile& take, int tick, double lateMs, double speed = 1.0)
{
    const double heard = 2.0 + calibrationTestTickToSecs(tick) / speed + lateMs / 1000.0;
    take.events.push_back({ calibrationTestNs(heard), true, 60, 90 });
    take.events.push_back({ calibrationTestNs(heard + 0.2), false, 60, 0 });
}

TEST_F(MidiRecording_CalibrationTests, SteadyLateNotesGiveTheirLateness)
{
    TakeFile take = calibrationTestTake();
    for (int beat = 0; beat < 8; ++beat) {
        calibrationTestPlay(take, 480 * beat, 38.0);
    }

    const CalibrationResult result = measureCalibration(take, 3840);

    EXPECT_EQ(result.notes, 8);
    EXPECT_NEAR(result.offsetMs, 38.0, 0.5);
    EXPECT_NEAR(result.spreadMs, 0.0, 0.5);
}

TEST_F(MidiRecording_CalibrationTests, OffsetIsTheMedianAndSpreadTheInterquartileRange)
{
    TakeFile take = calibrationTestTake();
    const double late[] = { 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0 };
    for (int beat = 0; beat < 8; ++beat) {
        calibrationTestPlay(take, 480 * beat, late[beat]);
    }

    const CalibrationResult result = measureCalibration(take, 3840);

    EXPECT_EQ(result.notes, 8);
    EXPECT_NEAR(result.offsetMs, 45.0, 0.5);
    // Quartiles between neighbours: 27.5 and 62.5
    EXPECT_NEAR(result.spreadMs, 35.0, 0.5);
}

TEST_F(MidiRecording_CalibrationTests, EarlyNotesGiveANegativeOffset)
{
    TakeFile take = calibrationTestTake();
    for (int beat = 0; beat < 8; ++beat) {
        calibrationTestPlay(take, 480 * beat, -20.0);
    }

    EXPECT_NEAR(measureCalibration(take, 3840).offsetMs, -20.0, 0.5);
}

TEST_F(MidiRecording_CalibrationTests, NotesOutsideTheTwoBarsAreLeftOut)
{
    TakeFile take = calibrationTestTake();
    // With the last two count-in clicks, before playback has moved
    calibrationTestPlay(take, -960, 0.0);
    calibrationTestPlay(take, -480, 0.0);
    for (int beat = 0; beat < 8; ++beat) {
        calibrationTestPlay(take, 480 * beat, 38.0);
    }
    // On the downbeat after the two bars
    calibrationTestPlay(take, 3840, 38.0);

    const CalibrationResult result = measureCalibration(take, 3840);

    EXPECT_EQ(result.notes, 8);
    EXPECT_NEAR(result.offsetMs, 38.0, 0.5);
    EXPECT_NEAR(result.spreadMs, 0.0, 0.5);
}

TEST_F(MidiRecording_CalibrationTests, LatenessIsRealTimeAtAnySpeed)
{
    TakeFile take = calibrationTestTake(0.5);
    for (int beat = 0; beat < 8; ++beat) {
        calibrationTestPlay(take, 480 * beat, 40.0, 0.5);
    }

    // 40 ms of real time is 20 ms of playback at half speed
    EXPECT_NEAR(measureCalibration(take, 3840).offsetMs, 40.0, 0.5);
}

TEST_F(MidiRecording_CalibrationTests, BeatsCountFromEachBarline)
{
    // A 3/8 measure between two 4/4 ones: beats counted from tick 0 would put the next downbeat at 2400, not 2640
    TakeFile take = calibrationTestTake();
    take.measures = { { 0, 1920, 4, 4 }, { 1920, 720, 3, 8 }, { 2640, 1920, 4, 4 } };
    const int ticks[] = { 0, 480, 960, 1440, 1920, 2400, 2640, 3120 };
    for (const int tick : ticks) {
        calibrationTestPlay(take, tick, 20.0);
    }

    const CalibrationResult result = measureCalibration(take, 4560);

    EXPECT_EQ(result.notes, 8);
    EXPECT_NEAR(result.offsetMs, 20.0, 0.5);
    EXPECT_NEAR(result.spreadMs, 0.0, 0.5);
}

TEST_F(MidiRecording_CalibrationTests, TakeWithoutAClockMeasuresNothing)
{
    TakeFile take = calibrationTestTake();
    for (int beat = 0; beat < 8; ++beat) {
        calibrationTestPlay(take, 480 * beat, 38.0);
    }
    take.clock.clear();

    EXPECT_EQ(measureCalibration(take, 3840).notes, 0);
}

TEST_F(MidiRecording_CalibrationTests, UsableNeedsFourNotesWithin30Ms)
{
    CalibrationResult result;
    result.notes = 4;
    result.offsetMs = 38.0;
    result.spreadMs = 30.0;
    EXPECT_TRUE(calibrationIsUsable(result));

    result.notes = 3;
    EXPECT_FALSE(calibrationIsUsable(result));

    result.notes = 8;
    result.spreadMs = 30.5;
    EXPECT_FALSE(calibrationIsUsable(result));
}
