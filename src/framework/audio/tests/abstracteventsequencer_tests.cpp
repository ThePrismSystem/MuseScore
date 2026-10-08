/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
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

//! Packed as in rpcpacker_tests.cpp: the tests of this executable share one layout of these types
#pragma pack(push, 1)
#include "audio/common/audiotypes.h"
#pragma pack(pop)

#include "audio/engine/internal/abstracteventsequencer.h"

using namespace muse;
using namespace muse::audio;

namespace {
class TestEventSequencer : public engine::AbstractEventSequencer<int>
{
public:
    void addOffStreamEvent(const msecs_t timestamp, const int event)
    {
        m_offStreamEvents[timestamp].push_back(event);
        updateOffSequenceIterator();
    }

    void addMainStreamEvent(const msecs_t timestamp, const int event)
    {
        m_mainStreamEvents[timestamp].push_back(event);
        updateMainSequenceIterator();
    }

protected:
    void updateOffStreamEvents(const mpe::PlaybackEventsMap&, const mpe::DynamicLevelLayers&) override {}
    void updateMainStreamEvents(const mpe::PlaybackEventsMap&, const mpe::DynamicLevelLayers&) override {}
};

constexpr msecs_t BUFFER_DURATION = 10000;
constexpr msecs_t PLAYBACK_POSITION = 1000000;
constexpr int OFFSTREAM_EVENT = 1;
constexpr int MAIN_STREAM_EVENT = 2;
}

TEST(Audio_AbstractEventSequencerTests, InactiveSequencerPlaysOffStreamEvents)
{
    TestEventSequencer sequencer;
    sequencer.addOffStreamEvent(0, OFFSTREAM_EVENT);

    const TestEventSequencer::EventSequenceMap result = sequencer.movePlaybackForward(BUFFER_DURATION);

    ASSERT_EQ(result.count(0), 1);
    EXPECT_EQ(result.at(0), TestEventSequencer::EventSequence { OFFSTREAM_EVENT });
}

//! Notes played on a MIDI keyboard while the score plays, e.g. during a MIDI recording take
TEST(Audio_AbstractEventSequencerTests, ActiveSequencerPlaysOffStreamEventsAtTheStartOfTheBuffer)
{
    TestEventSequencer sequencer;
    sequencer.setActive(true);
    sequencer.setPlaybackPosition(PLAYBACK_POSITION);
    sequencer.addMainStreamEvent(PLAYBACK_POSITION + BUFFER_DURATION / 2, MAIN_STREAM_EVENT);
    sequencer.addOffStreamEvent(0, OFFSTREAM_EVENT);

    const TestEventSequencer::EventSequenceMap result = sequencer.movePlaybackForward(BUFFER_DURATION);

    ASSERT_EQ(result.count(PLAYBACK_POSITION), 1);
    EXPECT_EQ(result.at(PLAYBACK_POSITION), TestEventSequencer::EventSequence { OFFSTREAM_EVENT });
    ASSERT_EQ(result.count(PLAYBACK_POSITION + BUFFER_DURATION / 2), 1);
    EXPECT_EQ(result.at(PLAYBACK_POSITION + BUFFER_DURATION / 2), TestEventSequencer::EventSequence { MAIN_STREAM_EVENT });
}

TEST(Audio_AbstractEventSequencerTests, ActiveSequencerHoldsAnOffStreamEventUntilItIsDue)
{
    constexpr msecs_t RELEASE_TIMESTAMP = 500000;
    constexpr int RELEASE_EVENT = 3;

    TestEventSequencer sequencer;
    sequencer.setActive(true);
    sequencer.setPlaybackPosition(PLAYBACK_POSITION);
    sequencer.addOffStreamEvent(0, OFFSTREAM_EVENT);
    sequencer.addOffStreamEvent(RELEASE_TIMESTAMP, RELEASE_EVENT);

    const TestEventSequencer::EventSequenceMap first = sequencer.movePlaybackForward(BUFFER_DURATION);
    ASSERT_EQ(first.count(PLAYBACK_POSITION), 1);
    EXPECT_EQ(first.at(PLAYBACK_POSITION), TestEventSequencer::EventSequence { OFFSTREAM_EVENT });

    const TestEventSequencer::EventSequenceMap second = sequencer.movePlaybackForward(RELEASE_TIMESTAMP - BUFFER_DURATION);
    ASSERT_EQ(second.count(PLAYBACK_POSITION + BUFFER_DURATION), 1);
    EXPECT_EQ(second.at(PLAYBACK_POSITION + BUFFER_DURATION), TestEventSequencer::EventSequence { RELEASE_EVENT });
}

//! A sequencer whose synth schedules its main stream itself takes only the offstream events
TEST(Audio_AbstractEventSequencerTests, MovingOffStreamForwardLeavesThePlaybackPosition)
{
    TestEventSequencer sequencer;
    sequencer.setActive(true);
    sequencer.setPlaybackPosition(PLAYBACK_POSITION);
    sequencer.addMainStreamEvent(PLAYBACK_POSITION, MAIN_STREAM_EVENT);
    sequencer.addOffStreamEvent(0, OFFSTREAM_EVENT);

    EXPECT_EQ(sequencer.moveOffStreamForward(BUFFER_DURATION), TestEventSequencer::EventSequence { OFFSTREAM_EVENT });
    EXPECT_EQ(sequencer.playbackPosition(), PLAYBACK_POSITION);
    EXPECT_TRUE(sequencer.moveOffStreamForward(BUFFER_DURATION).empty());
}
