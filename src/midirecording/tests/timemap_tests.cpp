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

#include "midirecording/internal/timemap.h"

using namespace mu::midirecording;

class MidiRecording_TimeMapTests : public ::testing::Test
{
};

//! 120 bpm: 960 ticks per second
static double timeMapTestSteady(int tick)
{
    return tick / 960.0;
}

//! 120 bpm for the first 4/4 measure, then 60 bpm
static double timeMapTestSlowsDown(int tick)
{
    return tick <= 1920 ? tick / 960.0 : 2.0 + (tick - 1920) / 480.0;
}

TEST_F(MidiRecording_TimeMapTests, BuildSamplesEveryStepAndEndsOnToTick)
{
    const std::vector<TimeKnot> knots = buildTimeMap(0, 300, 120, timeMapTestSteady);

    ASSERT_EQ(knots.size(), 4u);
    EXPECT_EQ(knots[0].tick, 0);
    EXPECT_EQ(knots[1].tick, 120);
    EXPECT_EQ(knots[2].tick, 240);
    EXPECT_EQ(knots[3].tick, 300);
    EXPECT_DOUBLE_EQ(knots[3].secs, 300 / 960.0);
}

TEST_F(MidiRecording_TimeMapTests, SteadyTempoMapsBack)
{
    const std::vector<TimeKnot> knots = buildTimeMap(0, 1920, 120, timeMapTestSteady);

    EXPECT_EQ(timeMapSecsToTick(knots, 0.0), 0);
    EXPECT_EQ(timeMapSecsToTick(knots, 0.1), 96);
    EXPECT_EQ(timeMapSecsToTick(knots, 0.5), 480);
    EXPECT_EQ(timeMapSecsToTick(knots, 1.0), 960);
    EXPECT_EQ(timeMapSecsToTick(knots, 2.0), 1920);
}

TEST_F(MidiRecording_TimeMapTests, TempoChangeIsFollowed)
{
    const std::vector<TimeKnot> knots = buildTimeMap(0, 3840, 120, timeMapTestSlowsDown);

    EXPECT_EQ(timeMapSecsToTick(knots, 1.0), 960);
    EXPECT_EQ(timeMapSecsToTick(knots, 2.0), 1920);
    EXPECT_EQ(timeMapSecsToTick(knots, 2.5), 2160);
    EXPECT_EQ(timeMapSecsToTick(knots, 3.0), 2400);
    EXPECT_EQ(timeMapSecsToTick(knots, 6.0), 3840);
}

TEST_F(MidiRecording_TimeMapTests, TimesPastEitherEndExtendTheEndSegments)
{
    const std::vector<TimeKnot> knots = buildTimeMap(960, 3840, 120, timeMapTestSlowsDown);

    // Before the first knot: the first segment's 120 bpm
    EXPECT_EQ(timeMapSecsToTick(knots, 0.9), 864);
    // Past the last knot: the last segment's 60 bpm
    EXPECT_EQ(timeMapSecsToTick(knots, 7.0), 4320);
}

TEST_F(MidiRecording_TimeMapTests, ValidityNeedsTwoRisingKnots)
{
    EXPECT_FALSE(timeMapIsValid({}));
    EXPECT_FALSE(timeMapIsValid({ { 0.0, 0 } }));
    EXPECT_TRUE(timeMapIsValid({ { 0.0, 0 }, { 1.0, 960 } }));
    EXPECT_FALSE(timeMapIsValid({ { 0.0, 0 }, { 0.0, 960 } }));
    EXPECT_FALSE(timeMapIsValid({ { 0.0, 0 }, { 1.0, 0 } }));
    EXPECT_FALSE(timeMapIsValid({ { 0.0, 0 }, { 1.0, 960 }, { 0.5, 1920 } }));
}

TEST_F(MidiRecording_TimeMapTests, TicksMapToSeconds)
{
    const std::vector<TimeKnot> knots = buildTimeMap(960, 3840, 120, timeMapTestSlowsDown);

    EXPECT_NEAR(timeMapTickToSecs(knots, 960), 1.0, 1e-9);
    // 60 ticks into the 60 bpm part: 2 s plus an eighth of a second
    EXPECT_NEAR(timeMapTickToSecs(knots, 1980), 2.125, 1e-9);
    EXPECT_NEAR(timeMapTickToSecs(knots, 2400), 3.0, 1e-9);
    // Before the first knot and past the last, the end segments extend
    EXPECT_NEAR(timeMapTickToSecs(knots, 864), 0.9, 1e-9);
    EXPECT_NEAR(timeMapTickToSecs(knots, 4320), 7.0, 1e-9);
}
