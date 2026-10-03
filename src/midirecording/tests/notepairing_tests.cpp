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

#include "midirecording/internal/notepairing.h"

using namespace mu::midirecording;

class MidiRecording_NotePairingTests : public ::testing::Test
{
};

//! One tick per millisecond keeps the expected numbers readable
static int notePairingTestNsToTick(int64_t ns)
{
    return static_cast<int>(ns / 1000000);
}

static RawEvent notePairingOn(int64_t ms, int pitch, int velocity = 100)
{
    return { ms* 1000000, true, pitch, velocity };
}

static RawEvent notePairingOff(int64_t ms, int pitch)
{
    return { ms* 1000000, false, pitch, 0 };
}

TEST_F(MidiRecording_NotePairingTests, PairsOnWithOff)
{
    const auto notes = pairNotes({ notePairingOn(100, 60), notePairingOff(400, 60) }, 1000000000, notePairingTestNsToTick);

    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0].pitch, 60);
    EXPECT_EQ(notes[0].onTick, 100);
    EXPECT_EQ(notes[0].offTick, 400);
    EXPECT_DOUBLE_EQ(notes[0].onMs, 100.0);
    EXPECT_DOUBLE_EQ(notes[0].heldMs, 300.0);
}

TEST_F(MidiRecording_NotePairingTests, VelocityZeroNoteOnIsNoteOff)
{
    const auto notes = pairNotes({ notePairingOn(100, 60), notePairingOn(300, 60, 0) }, 1000000000, notePairingTestNsToTick);

    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0].offTick, 300);
}

TEST_F(MidiRecording_NotePairingTests, OffWithoutOnIsIgnored)
{
    const auto notes = pairNotes({ notePairingOff(50, 62), notePairingOn(100, 60), notePairingOff(200, 60) },
                                 1000000000, notePairingTestNsToTick);

    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0].pitch, 60);
}

TEST_F(MidiRecording_NotePairingTests, RestrikeClosesHeldNote)
{
    const auto notes = pairNotes({ notePairingOn(0, 60), notePairingOn(200, 60), notePairingOff(500, 60) },
                                 1000000000, notePairingTestNsToTick);

    ASSERT_EQ(notes.size(), 2u);
    EXPECT_EQ(notes[0].onTick, 0);
    EXPECT_EQ(notes[0].offTick, 200);
    EXPECT_EQ(notes[1].onTick, 200);
    EXPECT_EQ(notes[1].offTick, 500);
}

TEST_F(MidiRecording_NotePairingTests, HeldAtStopEndsAtStop)
{
    const auto notes = pairNotes({ notePairingOn(100, 60) }, 900 * 1000000, notePairingTestNsToTick);

    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0].offTick, 900);
    EXPECT_DOUBLE_EQ(notes[0].heldMs, 800.0);
}

TEST_F(MidiRecording_NotePairingTests, EventsAfterStopAreIgnored)
{
    const auto notes = pairNotes({ notePairingOn(100, 60), notePairingOff(200, 60), notePairingOn(1000, 62) },
                                 900 * 1000000, notePairingTestNsToTick);

    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0].pitch, 60);
}

TEST_F(MidiRecording_NotePairingTests, OrderedByOnsetThenPitch)
{
    const auto notes = pairNotes({ notePairingOff(200, 64), notePairingOn(100, 64), notePairingOn(100, 60),
                                   notePairingOff(200, 60), notePairingOn(50, 67), notePairingOff(80, 67) },
                                 1000000000, notePairingTestNsToTick);

    ASSERT_EQ(notes.size(), 3u);
    EXPECT_EQ(notes[0].pitch, 67);
    EXPECT_EQ(notes[1].pitch, 60);
    EXPECT_EQ(notes[2].pitch, 64);
}
