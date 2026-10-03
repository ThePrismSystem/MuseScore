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

#include "midirecording/internal/takepipeline.h"

using namespace mu::midirecording;
using namespace muse;

class MidiRecording_TakePipelineTests : public ::testing::Test
{
};

static int64_t takePipelineTestNs(double secs)
{
    return static_cast<int64_t>(std::llround(secs * 1e9));
}

//! 120 bpm: 960 ticks per second
static int takePipelineTestSecsToTick(double secs)
{
    return static_cast<int>(std::lround(secs * 960.0));
}

static std::vector<MeasureSpan> takePipelineTestMeasures()
{
    return { { 0, 1920, 4, 4 }, { 1920, 1920, 4, 4 } };
}

TEST_F(MidiRecording_TakePipelineTests, RecordedQuartersComeBackAsQuarters)
{
    TakeFile take;
    take.latencyMs = 80.0;

    // Count-in: playback holds at 0 for two seconds, then runs in real time
    for (int i = 0; i < 200; ++i) {
        take.clock.push_back({ takePipelineTestNs(i * 0.01), 0.0 });
    }
    for (int i = 0; i <= 400; ++i) {
        const double t = 2.0 + i * 0.01;
        take.clock.push_back({ takePipelineTestNs(t), t - 2.0 });
    }

    // The player is consistently 80 ms late (77 ticks, past half a 16th), which the latency setting removes
    const int pitches[] = { 60, 62, 64, 65 };
    for (int i = 0; i < 4; ++i) {
        const double heard = 2.0 + 0.5 * i + 0.080;
        take.events.push_back({ takePipelineTestNs(heard), true, pitches[i], 90 });
        take.events.push_back({ takePipelineTestNs(heard + 0.45), false, pitches[i], 0 });
    }
    take.stopNs = takePipelineTestNs(6.0);

    const RetVal<QuantizeResult> result = quantizeTake(take, takePipelineTestMeasures(), takePipelineTestSecsToTick);

    ASSERT_TRUE(result.ret) << result.ret.text();
    ASSERT_EQ(result.val.events.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(result.val.events[i].startTick, 480 * i);
        EXPECT_EQ(result.val.events[i].ticks, 480);
        EXPECT_EQ(result.val.events[i].pitches, std::vector<int>({ pitches[i] }));
    }
    EXPECT_EQ(result.val.takeEndTick, 1920);
}

TEST_F(MidiRecording_TakePipelineTests, LatencyIsRemovedInRealTimeAtHalfSpeed)
{
    TakeFile take;
    take.latencyMs = 200.0;

    // Count-in for two seconds, then playback at half speed
    for (int i = 0; i < 200; ++i) {
        take.clock.push_back({ takePipelineTestNs(i * 0.01), 0.0 });
    }
    for (int i = 0; i <= 800; ++i) {
        const double t = 2.0 + i * 0.01;
        take.clock.push_back({ takePipelineTestNs(t), 0.5 * (t - 2.0) });
    }

    // Heard 200 ms of real time late: one second of real time is half a second of playback, a quarter at 120 bpm.
    // Subtracting the latency from playback seconds instead would put every onset 0.1 s (96 ticks) early.
    const int pitches[] = { 60, 62, 64, 65 };
    for (int k = 0; k < 4; ++k) {
        const double heard = 2.0 + 1.0 * k + 0.200;
        take.events.push_back({ takePipelineTestNs(heard), true, pitches[k], 90 });
        take.events.push_back({ takePipelineTestNs(heard + 0.9), false, pitches[k], 0 });
    }
    take.stopNs = takePipelineTestNs(8.0);

    const RetVal<QuantizeResult> result = quantizeTake(take, takePipelineTestMeasures(), takePipelineTestSecsToTick);

    ASSERT_TRUE(result.ret) << result.ret.text();
    ASSERT_EQ(result.val.events.size(), 4u);
    for (int k = 0; k < 4; ++k) {
        EXPECT_EQ(result.val.events[k].startTick, 480 * k);
        EXPECT_EQ(result.val.events[k].ticks, 480);
        EXPECT_EQ(result.val.events[k].pitches, std::vector<int>({ pitches[k] }));
    }
    EXPECT_EQ(result.val.takeEndTick, 1920);
}

TEST_F(MidiRecording_TakePipelineTests, TakeWithoutUsableClockIsAnError)
{
    TakeFile take;
    take.events.push_back({ takePipelineTestNs(1.0), true, 60, 90 });
    take.stopNs = takePipelineTestNs(2.0);

    EXPECT_FALSE(quantizeTake(take, takePipelineTestMeasures(), takePipelineTestSecsToTick).ret);
}

TEST_F(MidiRecording_TakePipelineTests, TakeWithOnlyCountInClockIsAnError)
{
    TakeFile take;
    for (int i = 0; i < 200; ++i) {
        take.clock.push_back({ takePipelineTestNs(i * 0.01), 0.0 });
    }
    take.events.push_back({ takePipelineTestNs(1.0), true, 60, 90 });
    take.events.push_back({ takePipelineTestNs(1.5), false, 60, 0 });
    take.stopNs = takePipelineTestNs(2.0);

    EXPECT_FALSE(quantizeTake(take, takePipelineTestMeasures(), takePipelineTestSecsToTick).ret);
}
