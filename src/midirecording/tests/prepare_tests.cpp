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

#include "midirecording/internal/prepare.h"

using namespace mu::midirecording;

class MidiRecording_PrepareTests : public ::testing::Test
{
};

static TimedNote prepareTestNote(int pitch, int onTick, int offTick, double onMs, double heldMs)
{
    TimedNote note;
    note.pitch = pitch;
    note.onTick = onTick;
    note.offTick = offTick;
    note.onMs = onMs;
    note.heldMs = heldMs;
    return note;
}

TEST_F(MidiRecording_PrepareTests, DropBrushesRemovesNotesShorterThanThreshold)
{
    const auto notes = dropBrushes({ prepareTestNote(60, 0, 40, 0.0, 39.9), prepareTestNote(62, 0, 40, 0.0, 40.0) }, 40.0);

    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0].pitch, 62);
}

TEST_F(MidiRecording_PrepareTests, AnticipationSnapsSlightlyEarlyNoteToStart)
{
    // Grid 120: up to 60 ticks early counts as on the start
    const auto notes = applyAnticipation({ prepareTestNote(60, 910, 950, 0.0, 50.0),
                                           prepareTestNote(62, 890, 1100, 0.0, 200.0),
                                           prepareTestNote(64, 1000, 1200, 0.0, 200.0) }, 960, 120);

    ASSERT_EQ(notes.size(), 2u);
    EXPECT_EQ(notes[0].pitch, 60);
    EXPECT_EQ(notes[0].onTick, 960);
    EXPECT_EQ(notes[0].offTick, 960);
    EXPECT_EQ(notes[1].pitch, 64);
    EXPECT_EQ(notes[1].onTick, 1000);
}

TEST_F(MidiRecording_PrepareTests, RolledChordSharesMedianOnset)
{
    const auto notes = groupChords({ prepareTestNote(60, 1000, 1400, 0.0, 400.0),
                                     prepareTestNote(64, 1010, 1400, 15.0, 400.0),
                                     prepareTestNote(67, 1020, 1400, 30.0, 400.0) }, 120);

    ASSERT_EQ(notes.size(), 3u);
    for (const TimedNote& note : notes) {
        EXPECT_EQ(note.onTick, 1010);
    }
}

TEST_F(MidiRecording_PrepareTests, OnsetsTooFarApartInTimeStaySeparate)
{
    const auto notes = groupChords({ prepareTestNote(60, 1000, 1400, 0.0, 400.0),
                                     prepareTestNote(64, 1010, 1400, 60.0, 400.0) }, 120);

    EXPECT_EQ(notes[0].onTick, 1000);
    EXPECT_EQ(notes[1].onTick, 1010);
}

TEST_F(MidiRecording_PrepareTests, OnsetsTooFarApartInTicksStaySeparate)
{
    // Half of a 120-tick grid step is 60 ticks
    const auto notes = groupChords({ prepareTestNote(60, 1000, 1400, 0.0, 400.0),
                                     prepareTestNote(64, 1070, 1400, 20.0, 400.0) }, 120);

    EXPECT_EQ(notes[0].onTick, 1000);
    EXPECT_EQ(notes[1].onTick, 1070);
}

TEST_F(MidiRecording_PrepareTests, ClusterIsMeasuredFromItsFirstOnset)
{
    // Every step is 50 ticks, within half a grid step, but 1150 is 150 ticks from the first onset,
    // past one grid step, so it starts a cluster of its own
    const auto notes = groupChords({ prepareTestNote(60, 1000, 1400, 0.0, 400.0),
                                     prepareTestNote(62, 1050, 1400, 10.0, 400.0),
                                     prepareTestNote(64, 1100, 1400, 20.0, 400.0),
                                     prepareTestNote(65, 1150, 1400, 30.0, 400.0) }, 120);

    EXPECT_EQ(notes[0].onTick, 1050);
    EXPECT_EQ(notes[1].onTick, 1050);
    EXPECT_EQ(notes[2].onTick, 1050);
    EXPECT_EQ(notes[3].onTick, 1150);
}
