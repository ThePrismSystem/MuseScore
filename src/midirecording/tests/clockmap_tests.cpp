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
#include <random>

#include "midirecording/internal/clockmap.h"

using namespace mu::midirecording;

class MidiRecording_ClockMapTests : public ::testing::Test
{
};

static int64_t clockMapTestNs(double secs)
{
    return static_cast<int64_t>(std::llround(secs * 1e9));
}

TEST_F(MidiRecording_ClockMapTests, InvalidUntilTwoSamplesAdvance)
{
    ClockMap map;
    EXPECT_FALSE(map.isValid());

    map.addSample({ clockMapTestNs(0.0), 0.0 });
    map.addSample({ clockMapTestNs(0.5), 0.0 });
    map.addSample({ clockMapTestNs(1.0), 0.5 });
    EXPECT_FALSE(map.isValid());
    EXPECT_DOUBLE_EQ(map.secsAt(clockMapTestNs(1.0)), 0.0);

    map.addSample({ clockMapTestNs(1.5), 1.0 });
    EXPECT_TRUE(map.isValid());
}

TEST_F(MidiRecording_ClockMapTests, ExactLineIsReproduced)
{
    ClockMap map;
    for (int i = 0; i <= 100; ++i) {
        const double t = i * 0.01;
        map.addSample({ clockMapTestNs(t), 2.0 + t });
    }

    ASSERT_TRUE(map.isValid());
    EXPECT_NEAR(map.slope(), 1.0, 1e-9);
    EXPECT_NEAR(map.secsAt(clockMapTestNs(1.5)), 3.5, 1e-9);
}

TEST_F(MidiRecording_ClockMapTests, CountInPlateauIsIgnored)
{
    ClockMap map;
    for (int i = 0; i < 200; ++i) {
        map.addSample({ clockMapTestNs(i * 0.01), 0.0 });
    }
    for (int i = 0; i <= 200; ++i) {
        const double t = 2.0 + i * 0.01;
        map.addSample({ clockMapTestNs(t), t - 2.0 });
    }

    ASSERT_TRUE(map.isValid());
    EXPECT_NEAR(map.slope(), 1.0, 1e-6);
    EXPECT_NEAR(map.secsAt(clockMapTestNs(2.5)), 0.5, 1e-6);
}

TEST_F(MidiRecording_ClockMapTests, LateArrivalsDoNotPullTheLineDown)
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> delay(0.0, 0.008);

    ClockMap map;
    map.addSample({ clockMapTestNs(0.0), 0.0 });
    for (int i = 1; i <= 600; ++i) {
        const double t = i * 0.005;
        map.addSample({ clockMapTestNs(t + delay(rng)), t });
    }

    // A mean fit would sit about 4 ms low; the high-percentile intercept stays within 1.5 ms
    ASSERT_TRUE(map.isValid());
    EXPECT_NEAR(map.secsAt(clockMapTestNs(2.0)), 2.0, 0.0015);
}

TEST_F(MidiRecording_ClockMapTests, SlopeOtherThanOneIsLearned)
{
    ClockMap map;
    for (int i = 0; i <= 100; ++i) {
        const double t = i * 0.05;
        map.addSample({ clockMapTestNs(t), 0.5 * t });
    }

    ASSERT_TRUE(map.isValid());
    EXPECT_NEAR(map.slope(), 0.5, 1e-9);
    EXPECT_NEAR(map.secsAt(clockMapTestNs(3.0)), 1.5, 1e-9);
}

TEST_F(MidiRecording_ClockMapTests, StaleSampleBeforeCountInIsIgnored)
{
    // A stale report at 0 ahead of a count-in held at 8 s: the jump to 8 s in 50 ms is too fast to be playback
    ClockMap map;
    map.addSample({ 0, 0.0 });
    for (int i = 1; i <= 20; ++i) {
        map.addSample({ clockMapTestNs(i * 0.05), 8.0 });
    }
    for (int i = 21; i <= 80; ++i) {
        map.addSample({ clockMapTestNs(i * 0.05), 8.0 + (i - 20) * 0.05 });
    }

    ASSERT_TRUE(map.isValid());
    EXPECT_NEAR(map.slope(), 1.0, 1e-9);
    EXPECT_NEAR(map.secsAt(clockMapTestNs(2.0)), 9.0, 1e-9);
}

TEST_F(MidiRecording_ClockMapTests, RepeatedReportMidTakeIsIgnored)
{
    // Playback runs in real time, but the report at 0.50 s repeats the position of the one before
    ClockMap map;
    for (int i = 0; i <= 100; ++i) {
        const int reported = i == 50 ? 49 : i;
        map.addSample({ clockMapTestNs(i * 0.01), reported * 0.01 });
    }

    ASSERT_TRUE(map.isValid());
    EXPECT_NEAR(map.slope(), 1.0, 1e-9);
    EXPECT_NEAR(map.secsAt(clockMapTestNs(0.75)), 0.75, 1e-9);
}

TEST_F(MidiRecording_ClockMapTests, PlateauOnlyIsInvalid)
{
    ClockMap map;
    for (int i = 0; i < 100; ++i) {
        map.addSample({ clockMapTestNs(i * 0.01), 3.0 });
    }

    EXPECT_FALSE(map.isValid());
    EXPECT_DOUBLE_EQ(map.slope(), 0.0);
    EXPECT_DOUBLE_EQ(map.secsAt(clockMapTestNs(0.5)), 0.0);
}

TEST_F(MidiRecording_ClockMapTests, FitThatRunsBackwardIsInvalid)
{
    // A second forward from 10 s, a seek back to 0, then another second forward: every sample after
    // the seek advances, but the line through both runs falls
    ClockMap map;
    for (int i = 0; i <= 20; ++i) {
        map.addSample({ clockMapTestNs(i * 0.05), 10.0 + i * 0.05 });
    }
    for (int i = 0; i <= 20; ++i) {
        map.addSample({ clockMapTestNs(1.05 + i * 0.05), i * 0.05 });
    }

    EXPECT_FALSE(map.isValid());
}

TEST_F(MidiRecording_ClockMapTests, ClearForgetsSamples)
{
    ClockMap map;
    for (int i = 0; i <= 10; ++i) {
        map.addSample({ clockMapTestNs(i * 0.1), i * 0.1 });
    }
    ASSERT_TRUE(map.isValid());

    map.clear();
    EXPECT_FALSE(map.isValid());
    EXPECT_TRUE(map.samples().empty());
}
