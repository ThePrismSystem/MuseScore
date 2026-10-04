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

#include "midirecording/internal/takesetup.h"

using namespace mu::midirecording;

class MidiRecording_TakeSetupTests : public ::testing::Test
{
};

TEST_F(MidiRecording_TakeSetupTests, StartOnTheGridStaysPut)
{
    EXPECT_EQ(takeStartTick(2400, 1920, 120), 2400);
    EXPECT_EQ(takeStartTick(1920, 1920, 120), 1920);
}

TEST_F(MidiRecording_TakeSetupTests, StartOffTheGridMovesBack)
{
    // After a dotted 16th, on a 16th grid
    EXPECT_EQ(takeStartTick(2100, 1920, 120), 2040);
}

TEST_F(MidiRecording_TakeSetupTests, StartCountsTheGridFromTheBarline)
{
    // A measure starting after an eighth-note pickup, on a quarter grid
    EXPECT_EQ(takeStartTick(740, 240, 480), 720);
}

TEST_F(MidiRecording_TakeSetupTests, SpanLeavesOutTheWholeStaff)
{
    EXPECT_EQ(takeExcludedTracks({ 1 }, 2, "span", true, 3), std::set<size_t>({ 4, 5, 6, 7 }));
}

TEST_F(MidiRecording_TakeSetupTests, VoiceLeavesOutOnlyThatVoice)
{
    EXPECT_EQ(takeExcludedTracks({ 1 }, 2, "voice", true, 3), std::set<size_t>({ 6 }));
}

TEST_F(MidiRecording_TakeSetupTests, ClickOnlyLeavesOutEveryTrack)
{
    const std::set<size_t> tracks = takeExcludedTracks({ 1 }, 2, "span", false, 3);

    EXPECT_EQ(tracks.size(), 12u);
    EXPECT_EQ(*tracks.begin(), 0u);
    EXPECT_EQ(*tracks.rbegin(), 11u);
}

TEST_F(MidiRecording_TakeSetupTests, LinkedStavesAreLeftOutToo)
{
    // Staff 0, with a TAB staff linked to it at staff 2
    EXPECT_EQ(takeExcludedTracks({ 0, 2 }, 1, "span", true, 3), std::set<size_t>({ 0, 1, 2, 3, 8, 9, 10, 11 }));
    EXPECT_EQ(takeExcludedTracks({ 0, 2 }, 1, "voice", true, 3), std::set<size_t>({ 1, 9 }));
}
