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

#include "midirecording/internal/notatedtext.h"

using namespace mu::midirecording;

class MidiRecording_NotatedTextTests : public ::testing::Test
{
};

static NotatedEvent notatedTextTestEvent(int startTick, int ticks, const std::vector<int>& pitches)
{
    NotatedEvent event;
    event.startTick = startTick;
    event.ticks = ticks;
    event.pitches = pitches;
    return event;
}

TEST_F(MidiRecording_NotatedTextTests, EveryKindOfEventHasItsLine)
{
    QuantizeResult result;
    result.takeStartTick = 0;
    result.takeEndTick = 1920;

    result.events.push_back(notatedTextTestEvent(0, 480, { 60, 64 }));

    NotatedEvent tied = notatedTextTestEvent(480, 480, { 60, 67 });
    tied.tiedFromPrevious = { 60 };
    result.events.push_back(tied);

    TupletInfo tuplet;
    tuplet.groupStartTick = 960;
    tuplet.groupTicks = 480;
    tuplet.unitTicks = 160;
    const int tripletPitches[] = { 62, 64, 65 };
    for (int i = 0; i < 3; ++i) {
        NotatedEvent member = notatedTextTestEvent(960 + 160 * i, 160, { tripletPitches[i] });
        member.tuplet = tuplet;
        result.events.push_back(member);
    }

    result.events.push_back(notatedTextTestEvent(1440, 480, {}));

    EXPECT_EQ(notatedEventsText(result),
              "take 0 1920\n"
              "0 480 60,64\n"
              "480 480 60,67 tied 60\n"
              "960 160 62 tuplet 960 480 3:2 160\n"
              "1120 160 64 tuplet 960 480 3:2 160\n"
              "1280 160 65 tuplet 960 480 3:2 160\n"
              "1440 480 rest\n");
    EXPECT_EQ(struckNoteCount(result), 6);
}

TEST_F(MidiRecording_NotatedTextTests, EmptyTakeIsOneLine)
{
    EXPECT_EQ(notatedEventsText(QuantizeResult()), "take 0 0\n");
    EXPECT_EQ(struckNoteCount(QuantizeResult()), 0);
}
