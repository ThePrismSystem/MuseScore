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

#include "midirecording/internal/snap.h"

using namespace mu::midirecording;

class MidiRecording_SnapTests : public ::testing::Test
{
};

static TimedNote snapTestNote(int pitch, int onTick, int offTick)
{
    TimedNote note;
    note.pitch = pitch;
    note.onTick = onTick;
    note.offTick = offTick;
    return note;
}

static std::vector<GridWindow> snapTestStraight()
{
    return { { 0, 1920, 120, 0, false } };
}

static std::vector<GridWindow> snapTestTripletBeat()
{
    return { { 0, 480, 120, 0, false }, { 480, 960, 160, 480, true }, { 960, 1920, 120, 0, false } };
}

static QuantizeSettings snapTestSettings(bool tidyGaps)
{
    QuantizeSettings settings;
    settings.tidyGaps = tidyGaps;
    return settings;
}

TEST_F(MidiRecording_SnapTests, OnsetsAndReleasesGoToNearestLine)
{
    const auto notes = snapNotes({ snapTestNote(60, 130, 470) }, snapTestStraight(), snapTestSettings(false));

    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0].onTick, 120);
    EXPECT_EQ(notes[0].offTick, 480);
}

TEST_F(MidiRecording_SnapTests, TripletWindowUsesTripletLines)
{
    const auto notes = snapNotes({ snapTestNote(60, 650, 790) }, snapTestTripletBeat(), snapTestSettings(false));

    EXPECT_EQ(notes[0].onTick, 640);
    EXPECT_EQ(notes[0].offTick, 800);
}

TEST_F(MidiRecording_SnapTests, ZeroLengthBecomesOneUnit)
{
    const auto notes = snapNotes({ snapTestNote(60, 118, 125) }, snapTestStraight(), snapTestSettings(false));

    EXPECT_EQ(notes[0].onTick, 120);
    EXPECT_EQ(notes[0].offTick, 240);
}

TEST_F(MidiRecording_SnapTests, SamePitchOverlapIsTrimmed)
{
    const auto notes = snapNotes({ snapTestNote(60, 0, 300), snapTestNote(60, 240, 600) },
                                 snapTestStraight(), snapTestSettings(false));

    ASSERT_EQ(notes.size(), 2u);
    EXPECT_EQ(notes[0].offTick, 240);
    EXPECT_EQ(notes[1].onTick, 240);
    EXPECT_EQ(notes[1].offTick, 600);
}

TEST_F(MidiRecording_SnapTests, DoubleStrikeOnOneLineMerges)
{
    const auto notes = snapNotes({ snapTestNote(60, 5, 100), snapTestNote(60, 20, 300) },
                                 snapTestStraight(), snapTestSettings(false));

    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0].onTick, 0);
    EXPECT_EQ(notes[0].offTick, 360);
}

TEST_F(MidiRecording_SnapTests, TidyGapsClosesShortGap)
{
    const auto notes = snapNotes({ snapTestNote(60, 0, 380), snapTestNote(62, 480, 960) },
                                 snapTestStraight(), snapTestSettings(true));

    EXPECT_EQ(notes[0].offTick, 480);
}

TEST_F(MidiRecording_SnapTests, TidyGapsLeavesLongGap)
{
    const auto notes = snapNotes({ snapTestNote(60, 0, 300), snapTestNote(62, 480, 960) },
                                 snapTestStraight(), snapTestSettings(true));

    EXPECT_EQ(notes[0].offTick, 360);
}

TEST_F(MidiRecording_SnapTests, TidyGapsOffSnapsNormally)
{
    const auto notes = snapNotes({ snapTestNote(60, 0, 380), snapTestNote(62, 480, 960) },
                                 snapTestStraight(), snapTestSettings(false));

    EXPECT_EQ(notes[0].offTick, 360);
}

TEST_F(MidiRecording_SnapTests, OrderedByOnsetThenPitch)
{
    const auto notes = snapNotes({ snapTestNote(67, 480, 960), snapTestNote(64, 0, 480), snapTestNote(60, 0, 480) },
                                 snapTestStraight(), snapTestSettings(false));

    ASSERT_EQ(notes.size(), 3u);
    EXPECT_EQ(notes[0].pitch, 60);
    EXPECT_EQ(notes[1].pitch, 64);
    EXPECT_EQ(notes[2].pitch, 67);
}

TEST_F(MidiRecording_SnapTests, TidyGapsDoesNotPullBackAnOverlappingRelease)
{
    // The raw release is already past the next onset, so tidying must not move it back onto that onset
    const auto notes = snapNotes({ snapTestNote(60, 0, 560), snapTestNote(62, 480, 960) },
                                 snapTestStraight(), snapTestSettings(true));

    EXPECT_EQ(notes[0].offTick, 600);
}

TEST_F(MidiRecording_SnapTests, TidyGapsLeavesGapOfExactlyOneStep)
{
    const auto notes = snapNotes({ snapTestNote(60, 0, 360), snapTestNote(62, 480, 960) },
                                 snapTestStraight(), snapTestSettings(true));

    EXPECT_EQ(notes[0].offTick, 360);
}

TEST_F(MidiRecording_SnapTests, ZeroLengthInTripletWindowBecomesOneTripletUnit)
{
    const auto notes = snapNotes({ snapTestNote(60, 645, 650) }, snapTestTripletBeat(), snapTestSettings(false));

    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0].onTick, 640);
    EXPECT_EQ(notes[0].offTick, 800);
}

TEST_F(MidiRecording_SnapTests, TidyGapsReachesAnOnsetInAnotherWindow)
{
    const auto notes = snapNotes({ snapTestNote(60, 0, 400), snapTestNote(62, 480, 640) },
                                 snapTestTripletBeat(), snapTestSettings(true));

    ASSERT_EQ(notes.size(), 2u);
    EXPECT_EQ(notes[0].offTick, 480);
    EXPECT_EQ(notes[1].offTick, 640);
}

TEST_F(MidiRecording_SnapTests, DifferentPitchesOnOneLineStaySeparate)
{
    const auto notes = snapNotes({ snapTestNote(64, 10, 475), snapTestNote(60, 5, 470) },
                                 snapTestStraight(), snapTestSettings(false));

    ASSERT_EQ(notes.size(), 2u);
    EXPECT_EQ(notes[0].pitch, 60);
    EXPECT_EQ(notes[1].pitch, 64);
    for (const TimedNote& note : notes) {
        EXPECT_EQ(note.onTick, 0);
        EXPECT_EQ(note.offTick, 480);
    }
}

TEST_F(MidiRecording_SnapTests, ThreeOverlappingStrikesOfOnePitchAreTrimmedInTurn)
{
    const auto notes = snapNotes({ snapTestNote(60, 0, 400), snapTestNote(60, 230, 600), snapTestNote(60, 470, 700) },
                                 snapTestStraight(), snapTestSettings(false));

    ASSERT_EQ(notes.size(), 3u);
    EXPECT_EQ(notes[0].onTick, 0);
    EXPECT_EQ(notes[0].offTick, 240);
    EXPECT_EQ(notes[1].onTick, 240);
    EXPECT_EQ(notes[1].offTick, 480);
    EXPECT_EQ(notes[2].onTick, 480);
    EXPECT_EQ(notes[2].offTick, 720);
}
