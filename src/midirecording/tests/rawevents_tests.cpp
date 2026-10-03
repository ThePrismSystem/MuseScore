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

#include "midirecording/internal/rawevents.h"

using namespace mu::midirecording;
using namespace muse::midi;

class MidiRecording_RawEventsTests : public ::testing::Test
{
};

static Event rawEventsTestMidi10(Event::Opcode opcode, uint8_t note, uint8_t velocity)
{
    Event event(opcode, Event::MessageType::ChannelVoice10);
    event.setNote(note);
    event.setVelocity7(velocity);
    return event;
}

TEST_F(MidiRecording_RawEventsTests, NoteOnBecomesOn)
{
    const std::vector<RawEvent> events = rawEventsFromMidi(123, rawEventsTestMidi10(Event::Opcode::NoteOn, 60, 100));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].ns, 123);
    EXPECT_TRUE(events[0].on);
    EXPECT_EQ(events[0].pitch, 60);
    EXPECT_EQ(events[0].velocity, 100);
}

TEST_F(MidiRecording_RawEventsTests, NoteOffBecomesOff)
{
    const std::vector<RawEvent> events = rawEventsFromMidi(5, rawEventsTestMidi10(Event::Opcode::NoteOff, 61, 64));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_FALSE(events[0].on);
    EXPECT_EQ(events[0].pitch, 61);
    EXPECT_EQ(events[0].velocity, 0);
}

TEST_F(MidiRecording_RawEventsTests, NoteOnWithZeroVelocityIsOff)
{
    const std::vector<RawEvent> events = rawEventsFromMidi(5, rawEventsTestMidi10(Event::Opcode::NoteOn, 62, 0));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_FALSE(events[0].on);
    EXPECT_EQ(events[0].pitch, 62);
}

TEST_F(MidiRecording_RawEventsTests, Midi20NoteOnKeepsItsPitchAndVelocity)
{
    Event event(Event::Opcode::NoteOn, Event::MessageType::ChannelVoice20);
    event.setNote(64);
    event.setVelocity16(0x8000);

    const std::vector<RawEvent> events = rawEventsFromMidi(7, event);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_TRUE(events[0].on);
    EXPECT_EQ(events[0].pitch, 64);
    EXPECT_EQ(events[0].velocity, 64);
}

//! CoreMIDI's legacy receive path turns every message into MIDI 2.0, and a
//! MIDI 1.0 note-on of velocity 0 becomes a 2.0 note-off on the way
TEST_F(MidiRecording_RawEventsTests, ZeroVelocityNoteOnStaysOffThroughMidi20)
{
    const Event midi20 = rawEventsTestMidi10(Event::Opcode::NoteOn, 65, 0).toMIDI20();

    const std::vector<RawEvent> events = rawEventsFromMidi(9, midi20);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_FALSE(events[0].on);
    EXPECT_EQ(events[0].pitch, 65);
}

TEST_F(MidiRecording_RawEventsTests, OtherMessagesGiveNothing)
{
    EXPECT_TRUE(rawEventsFromMidi(1, Event(Event::Opcode::ControlChange, Event::MessageType::ChannelVoice10)).empty());
    EXPECT_TRUE(rawEventsFromMidi(1, Event(Event::Opcode::PitchBend, Event::MessageType::ChannelVoice20)).empty());
    EXPECT_TRUE(rawEventsFromMidi(1, Event()).empty());
}
