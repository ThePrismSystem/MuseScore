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

#include "midirecording/internal/voicebuilder.h"

using namespace mu::midirecording;

class MidiRecording_VoiceBuilderTests : public ::testing::Test
{
};

static TimedNote voiceTestNote(int pitch, int onTick, int offTick)
{
    TimedNote note;
    note.pitch = pitch;
    note.onTick = onTick;
    note.offTick = offTick;
    return note;
}

static std::vector<GridWindow> voiceTestStraight()
{
    return { { 0, 1920, 120, 0, false } };
}

static NotatedEvent voiceTestEvent(int start, int ticks, std::vector<int> pitches)
{
    NotatedEvent event;
    event.startTick = start;
    event.ticks = ticks;
    event.pitches = pitches;
    return event;
}

static void expectVoiceEvent(const NotatedEvent& event, int start, int ticks, const std::vector<int>& pitches,
                             const std::vector<int>& tied)
{
    EXPECT_EQ(event.startTick, start);
    EXPECT_EQ(event.ticks, ticks);
    EXPECT_EQ(event.pitches, pitches);
    EXPECT_EQ(event.tiedFromPrevious, tied);
}

TEST_F(MidiRecording_VoiceBuilderTests, SingleNoteIsSurroundedByRests)
{
    const auto events = buildVoice({ voiceTestNote(60, 480, 960) }, voiceTestStraight(), 0, 1920, OverlapMode::Tied);

    ASSERT_EQ(events.size(), 3u);
    expectVoiceEvent(events[0], 0, 480, {}, {});
    expectVoiceEvent(events[1], 480, 480, { 60 }, {});
    expectVoiceEvent(events[2], 960, 960, {}, {});
}

TEST_F(MidiRecording_VoiceBuilderTests, ChordNotesShareOneEvent)
{
    const auto events = buildVoice({ voiceTestNote(67, 0, 480), voiceTestNote(60, 0, 480), voiceTestNote(64, 0, 480) },
                                   voiceTestStraight(), 0, 1920, OverlapMode::Tied);

    ASSERT_EQ(events.size(), 2u);
    expectVoiceEvent(events[0], 0, 480, { 60, 64, 67 }, {});
}

TEST_F(MidiRecording_VoiceBuilderTests, TiedModeContinuesHeldNote)
{
    const std::vector<TimedNote> notes { voiceTestNote(48, 0, 1920), voiceTestNote(60, 0, 240),
                                         voiceTestNote(62, 240, 480), voiceTestNote(64, 480, 960) };

    const auto events = buildVoice(notes, voiceTestStraight(), 0, 1920, OverlapMode::Tied);

    ASSERT_EQ(events.size(), 4u);
    expectVoiceEvent(events[0], 0, 240, { 48, 60 }, {});
    expectVoiceEvent(events[1], 240, 240, { 48, 62 }, { 48 });
    expectVoiceEvent(events[2], 480, 480, { 48, 64 }, { 48 });
    expectVoiceEvent(events[3], 960, 960, { 48 }, { 48 });
}

TEST_F(MidiRecording_VoiceBuilderTests, CutModeEndsHeldNoteAtNextOnset)
{
    const std::vector<TimedNote> notes { voiceTestNote(48, 0, 1920), voiceTestNote(60, 0, 240),
                                         voiceTestNote(62, 240, 480), voiceTestNote(64, 480, 960) };

    const auto events = buildVoice(notes, voiceTestStraight(), 0, 1920, OverlapMode::Cut);

    ASSERT_EQ(events.size(), 4u);
    expectVoiceEvent(events[0], 0, 240, { 48, 60 }, {});
    expectVoiceEvent(events[1], 240, 240, { 62 }, {});
    expectVoiceEvent(events[2], 480, 480, { 64 }, {});
    expectVoiceEvent(events[3], 960, 960, {}, {});
}

TEST_F(MidiRecording_VoiceBuilderTests, CutModeChordTakesLongestLength)
{
    const auto events = buildVoice({ voiceTestNote(60, 0, 240), voiceTestNote(64, 0, 480) },
                                   voiceTestStraight(), 0, 1920, OverlapMode::Cut);

    ASSERT_EQ(events.size(), 2u);
    expectVoiceEvent(events[0], 0, 480, { 60, 64 }, {});
}

TEST_F(MidiRecording_VoiceBuilderTests, RestrikeIsNotTied)
{
    const auto events = buildVoice({ voiceTestNote(60, 0, 480), voiceTestNote(60, 480, 960) },
                                   voiceTestStraight(), 0, 1920, OverlapMode::Tied);

    ASSERT_EQ(events.size(), 3u);
    expectVoiceEvent(events[1], 480, 480, { 60 }, {});
}

TEST_F(MidiRecording_VoiceBuilderTests, TripletWindowSplitsEventsAtItsEdges)
{
    const std::vector<GridWindow> windows { { 0, 480, 120, 0, false }, { 480, 960, 160, 480, true },
        { 960, 1920, 120, 0, false } };
    const std::vector<TimedNote> notes { voiceTestNote(60, 0, 640), voiceTestNote(62, 640, 800), voiceTestNote(64, 800, 960) };

    const auto events = buildVoice(notes, windows, 0, 1920, OverlapMode::Tied);

    const TupletInfo group { 480, 480, 3, 2, 160 };
    ASSERT_EQ(events.size(), 5u);
    expectVoiceEvent(events[0], 0, 480, { 60 }, {});
    EXPECT_FALSE(events[0].tuplet.has_value());
    expectVoiceEvent(events[1], 480, 160, { 60 }, { 60 });
    EXPECT_EQ(events[1].tuplet, group);
    expectVoiceEvent(events[2], 640, 160, { 62 }, {});
    EXPECT_EQ(events[2].tuplet, group);
    expectVoiceEvent(events[3], 800, 160, { 64 }, {});
    EXPECT_EQ(events[3].tuplet, group);
    expectVoiceEvent(events[4], 960, 960, {}, {});
    EXPECT_FALSE(events[4].tuplet.has_value());
}

TEST_F(MidiRecording_VoiceBuilderTests, EmptyTakeIsOneRest)
{
    const auto events = buildVoice({}, voiceTestStraight(), 0, 1920, OverlapMode::Tied);

    ASSERT_EQ(events.size(), 1u);
    expectVoiceEvent(events[0], 0, 1920, {}, {});
}

TEST_F(MidiRecording_VoiceBuilderTests, ShortRestFoldsIntoPreviousChord)
{
    const auto events = absorbShortRests({ voiceTestEvent(0, 360, { 60 }), voiceTestEvent(360, 120, {}),
                                           voiceTestEvent(480, 480, { 62 }), voiceTestEvent(960, 960, {}) }, 240);

    ASSERT_EQ(events.size(), 3u);
    expectVoiceEvent(events[0], 0, 480, { 60 }, {});
    expectVoiceEvent(events[1], 480, 480, { 62 }, {});
    expectVoiceEvent(events[2], 960, 960, {}, {});
}

TEST_F(MidiRecording_VoiceBuilderTests, LeadingShortRestIsKept)
{
    const auto events = absorbShortRests({ voiceTestEvent(0, 120, {}), voiceTestEvent(120, 360, { 60 }) }, 240);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_TRUE(events[0].isRest());
}

TEST_F(MidiRecording_VoiceBuilderTests, ShortRestInTripletGroupIsKept)
{
    NotatedEvent rest = voiceTestEvent(640, 160, {});
    rest.tuplet = TupletInfo { 480, 480, 3, 2, 160 };
    NotatedEvent chord = voiceTestEvent(480, 160, { 60 });
    chord.tuplet = rest.tuplet;

    const auto events = absorbShortRests({ chord, rest }, 240);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_TRUE(events[1].isRest());
}
