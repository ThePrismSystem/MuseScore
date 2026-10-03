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

#include "midirecording/internal/gridselection.h"

using namespace mu::midirecording;

class MidiRecording_GridSelectionTests : public ::testing::Test
{
};

static std::vector<MeasureSpan> gridTestMeasures44(int count)
{
    std::vector<MeasureSpan> measures;
    for (int i = 0; i < count; ++i) {
        measures.push_back({ i* 1920, 1920, 4, 4 });
    }
    return measures;
}

static TimedNote gridTestNote(int onTick, int offTick)
{
    TimedNote note;
    note.pitch = 60;
    note.onTick = onTick;
    note.offTick = offTick;
    return note;
}

TEST_F(MidiRecording_GridSelectionTests, NearestGridLineRoundsHalfUp)
{
    const GridWindow window { 0, 1920, 120, 0, false };
    EXPECT_EQ(nearestGridLine(59, window), 0);
    EXPECT_EQ(nearestGridLine(60, window), 120);
    EXPECT_EQ(nearestGridLine(-10, window), 0);
}

TEST_F(MidiRecording_GridSelectionTests, WindowAtClampsToEnds)
{
    const std::vector<GridWindow> windows { { 0, 480, 120, 0, false }, { 480, 960, 160, 480, true } };
    EXPECT_EQ(windowAt(windows, -5), windows[0]);
    EXPECT_EQ(windowAt(windows, 479), windows[0]);
    EXPECT_EQ(windowAt(windows, 480), windows[1]);
    EXPECT_EQ(windowAt(windows, 5000), windows[1]);
}

TEST_F(MidiRecording_GridSelectionTests, StraightEighthsStayStraight)
{
    std::vector<TimedNote> notes;
    for (int tick = 0; tick < 1920; tick += 240) {
        notes.push_back(gridTestNote(tick, tick + 240));
    }

    const auto windows = chooseGrids(notes, gridTestMeasures44(1), 0, 1920, QuantizeSettings());

    ASSERT_EQ(windows.size(), 4u);
    for (const GridWindow& window : windows) {
        EXPECT_FALSE(window.triplet);
        EXPECT_EQ(window.unitTicks, 120);
        EXPECT_EQ(window.anchorTick, 0);
    }
}

TEST_F(MidiRecording_GridSelectionTests, TripletBeatIsDetected)
{
    const std::vector<TimedNote> notes { gridTestNote(0, 240), gridTestNote(240, 480),
                                         gridTestNote(480, 640), gridTestNote(640, 800), gridTestNote(800, 960),
                                         gridTestNote(960, 1440), gridTestNote(1440, 1920) };

    const auto windows = chooseGrids(notes, gridTestMeasures44(1), 0, 1920, QuantizeSettings());

    ASSERT_EQ(windows.size(), 4u);
    EXPECT_FALSE(windows[0].triplet);
    EXPECT_EQ(windows[1], (GridWindow { 480, 960, 160, 480, true }));
    EXPECT_FALSE(windows[2].triplet);
    EXPECT_FALSE(windows[3].triplet);
}

TEST_F(MidiRecording_GridSelectionTests, JitteredTripletBeatIsDetected)
{
    const std::vector<TimedNote> notes { gridTestNote(485, 630), gridTestNote(630, 812), gridTestNote(812, 960) };

    const auto windows = chooseGrids(notes, gridTestMeasures44(1), 0, 1920, QuantizeSettings());

    EXPECT_TRUE(windows[1].triplet);
}

TEST_F(MidiRecording_GridSelectionTests, OneOffBeatOnsetIsNotEnough)
{
    const std::vector<TimedNote> notes { gridTestNote(480, 640), gridTestNote(640, 960) };

    const auto windows = chooseGrids(notes, gridTestMeasures44(1), 0, 1920, QuantizeSettings());

    EXPECT_FALSE(windows[1].triplet);
}

TEST_F(MidiRecording_GridSelectionTests, TripletsOffGivesOneWindowPerMeasure)
{
    QuantizeSettings settings;
    settings.triplets = false;
    const std::vector<TimedNote> notes { gridTestNote(480, 640), gridTestNote(640, 800), gridTestNote(800, 960) };

    const auto windows = chooseGrids(notes, gridTestMeasures44(2), 0, 3840, settings);

    ASSERT_EQ(windows.size(), 2u);
    EXPECT_EQ(windows[0], (GridWindow { 0, 1920, 120, 0, false }));
    EXPECT_EQ(windows[1], (GridWindow { 1920, 3840, 120, 1920, false }));
}

TEST_F(MidiRecording_GridSelectionTests, CompoundMeterNeverGoesTriplet)
{
    const std::vector<MeasureSpan> measures { { 0, 1440, 6, 8 } };
    const std::vector<TimedNote> notes { gridTestNote(0, 160), gridTestNote(160, 320), gridTestNote(320, 480) };

    const auto windows = chooseGrids(notes, measures, 0, 1440, QuantizeSettings());

    ASSERT_EQ(windows.size(), 1u);
    EXPECT_EQ(windows[0], (GridWindow { 0, 1440, 120, 0, false }));
}

TEST_F(MidiRecording_GridSelectionTests, MeasureNotDivisibleIsOneStraightWindow)
{
    const std::vector<MeasureSpan> measures { { 0, 1200, 5, 8 } };

    const auto windows = chooseGrids({}, measures, 0, 1200, QuantizeSettings());

    ASSERT_EQ(windows.size(), 1u);
    EXPECT_EQ(windows[0], (GridWindow { 0, 1200, 120, 0, false }));
}

TEST_F(MidiRecording_GridSelectionTests, TimeSignatureChangeTilesEachMeasure)
{
    const std::vector<MeasureSpan> measures { { 0, 1920, 4, 4 }, { 1920, 1440, 3, 4 } };

    const auto windows = chooseGrids({}, measures, 0, 3360, QuantizeSettings());

    ASSERT_EQ(windows.size(), 7u);
    EXPECT_EQ(windows[4], (GridWindow { 1920, 2400, 120, 1920, false }));
    EXPECT_EQ(windows[6].endTick, 3360);
}

TEST_F(MidiRecording_GridSelectionTests, PickupStartKeepsMeasureAlignment)
{
    const auto windows = chooseGrids({}, gridTestMeasures44(1), 960, 1920, QuantizeSettings());

    ASSERT_EQ(windows.size(), 4u);
    EXPECT_EQ(windows[0].startTick, 0);
    EXPECT_EQ(windows[2].startTick, 960);
}

TEST_F(MidiRecording_GridSelectionTests, QuarterTripletsUseTwoBeatWindows)
{
    QuantizeSettings settings;
    settings.tripletUnitTicks = 320;
    const std::vector<TimedNote> notes { gridTestNote(0, 320), gridTestNote(320, 640), gridTestNote(640, 960) };

    const auto windows = chooseGrids(notes, gridTestMeasures44(1), 0, 1920, settings);

    ASSERT_EQ(windows.size(), 2u);
    EXPECT_EQ(windows[0], (GridWindow { 0, 960, 320, 0, true }));
    EXPECT_FALSE(windows[1].triplet);
}
