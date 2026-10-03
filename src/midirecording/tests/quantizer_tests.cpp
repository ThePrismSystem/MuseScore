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

#include "midirecording/internal/quantizer.h"

using namespace mu::midirecording;

class MidiRecording_QuantizerTests : public ::testing::Test
{
};

//! 120 bpm: a quarter note (480 ticks) lasts 500 ms
static constexpr double QUANTIZER_TEST_MS_PER_TICK = 500.0 / 480.0;

static TimedNote quantizerTestNote(int pitch, int onTick, int offTick)
{
    TimedNote note;
    note.pitch = pitch;
    note.onTick = onTick;
    note.offTick = offTick;
    note.onMs = onTick * QUANTIZER_TEST_MS_PER_TICK;
    note.heldMs = (offTick - onTick) * QUANTIZER_TEST_MS_PER_TICK;
    return note;
}

static std::vector<MeasureSpan> quantizerTestMeasures(int count)
{
    std::vector<MeasureSpan> measures;
    for (int i = 0; i < count; ++i) {
        measures.push_back({ i* 1920, 1920, 4, 4 });
    }
    return measures;
}

static void expectQuantizedEvent(const NotatedEvent& event, int start, int ticks, const std::vector<int>& pitches)
{
    EXPECT_EQ(event.startTick, start);
    EXPECT_EQ(event.ticks, ticks);
    EXPECT_EQ(event.pitches, pitches);
}

TEST_F(MidiRecording_QuantizerTests, EmptyTakeHasNoEvents)
{
    const QuantizeResult result = quantize({}, quantizerTestMeasures(2), 0, QuantizeSettings());

    EXPECT_TRUE(result.events.empty());
    EXPECT_EQ(result.takeEndTick, 0);
}

TEST_F(MidiRecording_QuantizerTests, JitteredQuartersBecomeQuarters)
{
    const std::vector<TimedNote> notes { quantizerTestNote(60, -10, 420), quantizerTestNote(62, 495, 900),
                                         quantizerTestNote(64, 948, 1390), quantizerTestNote(65, 1448, 1880) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_EQ(result.events.size(), 4u);
    expectQuantizedEvent(result.events[0], 0, 480, { 60 });
    expectQuantizedEvent(result.events[1], 480, 480, { 62 });
    expectQuantizedEvent(result.events[2], 960, 480, { 64 });
    expectQuantizedEvent(result.events[3], 1440, 480, { 65 });
    EXPECT_EQ(result.takeEndTick, 1920);
}

TEST_F(MidiRecording_QuantizerTests, LeadingRestBeforeFirstNote)
{
    const QuantizeResult result = quantize({ quantizerTestNote(60, 480, 960) }, quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_EQ(result.events.size(), 3u);
    expectQuantizedEvent(result.events[0], 0, 480, {});
    expectQuantizedEvent(result.events[1], 480, 480, { 60 });
    expectQuantizedEvent(result.events[2], 960, 960, {});
}

TEST_F(MidiRecording_QuantizerTests, TripletBeatBetweenStraightNotes)
{
    const std::vector<TimedNote> notes { quantizerTestNote(60, 0, 230), quantizerTestNote(62, 240, 470),
                                         quantizerTestNote(64, 480, 630), quantizerTestNote(65, 640, 790),
                                         quantizerTestNote(67, 800, 950), quantizerTestNote(69, 960, 1430),
                                         quantizerTestNote(71, 1440, 1900) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());

    const TupletInfo group { 480, 480, 3, 2, 160 };
    ASSERT_EQ(result.events.size(), 7u);
    expectQuantizedEvent(result.events[0], 0, 240, { 60 });
    expectQuantizedEvent(result.events[1], 240, 240, { 62 });
    expectQuantizedEvent(result.events[2], 480, 160, { 64 });
    expectQuantizedEvent(result.events[3], 640, 160, { 65 });
    expectQuantizedEvent(result.events[4], 800, 160, { 67 });
    expectQuantizedEvent(result.events[5], 960, 480, { 69 });
    expectQuantizedEvent(result.events[6], 1440, 480, { 71 });
    EXPECT_FALSE(result.events[1].tuplet.has_value());
    EXPECT_EQ(result.events[2].tuplet, group);
    EXPECT_EQ(result.events[4].tuplet, group);
    EXPECT_FALSE(result.events[5].tuplet.has_value());
}

TEST_F(MidiRecording_QuantizerTests, HeldNoteUnderMovingNotes)
{
    const std::vector<TimedNote> notes { quantizerTestNote(48, 0, 1900), quantizerTestNote(60, 0, 230),
                                         quantizerTestNote(62, 240, 470), quantizerTestNote(64, 480, 950) };

    const QuantizeResult tied = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());
    ASSERT_EQ(tied.events.size(), 4u);
    expectQuantizedEvent(tied.events[3], 960, 960, { 48 });
    EXPECT_EQ(tied.events[3].tiedFromPrevious, std::vector<int>({ 48 }));

    QuantizeSettings cutSettings;
    cutSettings.overlaps = OverlapMode::Cut;
    const QuantizeResult cut = quantize(notes, quantizerTestMeasures(2), 0, cutSettings);
    ASSERT_EQ(cut.events.size(), 4u);
    expectQuantizedEvent(cut.events[0], 0, 240, { 48, 60 });
    expectQuantizedEvent(cut.events[3], 960, 960, {});
}

TEST_F(MidiRecording_QuantizerTests, ShortRestsAreAbsorbed)
{
    QuantizeSettings settings;
    settings.tidyGaps = false;
    const std::vector<TimedNote> notes { quantizerTestNote(60, 0, 300), quantizerTestNote(62, 480, 1440) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, settings);

    ASSERT_EQ(result.events.size(), 3u);
    expectQuantizedEvent(result.events[0], 0, 480, { 60 });
    expectQuantizedEvent(result.events[1], 480, 960, { 62 });
    expectQuantizedEvent(result.events[2], 1440, 480, {});
}

TEST_F(MidiRecording_QuantizerTests, TakeEndIsEndOfMeasureHoldingLastRelease)
{
    const QuantizeResult result = quantize({ quantizerTestNote(60, 1920, 2000) }, quantizerTestMeasures(3), 0, QuantizeSettings());

    EXPECT_EQ(result.takeEndTick, 3840);
}

TEST_F(MidiRecording_QuantizerTests, ReleaseOnBarlineEndsThatMeasure)
{
    const QuantizeResult result = quantize({ quantizerTestNote(60, 960, 1915) }, quantizerTestMeasures(3), 0, QuantizeSettings());

    EXPECT_EQ(result.takeEndTick, 1920);
}

TEST_F(MidiRecording_QuantizerTests, TakePastLastMeasureExtendsMeasures)
{
    const QuantizeResult result = quantize({ quantizerTestNote(60, 2000, 2400) }, quantizerTestMeasures(1), 0, QuantizeSettings());

    EXPECT_EQ(result.takeEndTick, 3840);
    EXPECT_EQ(result.events.back().startTick + result.events.back().ticks, 3840);
}

TEST_F(MidiRecording_QuantizerTests, TimeSignatureChangeEndsAtThatMeasure)
{
    const std::vector<MeasureSpan> measures { { 0, 1920, 4, 4 }, { 1920, 1440, 3, 4 } };

    const QuantizeResult result = quantize({ quantizerTestNote(60, 1920, 3000) }, measures, 0, QuantizeSettings());

    EXPECT_EQ(result.takeEndTick, 3360);
}

TEST_F(MidiRecording_QuantizerTests, PickupStartWritesFromStart)
{
    const QuantizeResult result = quantize({ quantizerTestNote(60, 960, 1440) }, quantizerTestMeasures(2), 960, QuantizeSettings());

    EXPECT_EQ(result.takeStartTick, 960);
    ASSERT_FALSE(result.events.empty());
    expectQuantizedEvent(result.events[0], 960, 480, { 60 });
}

TEST_F(MidiRecording_QuantizerTests, AnticipatedFirstNoteLandsOnStart)
{
    const QuantizeResult result = quantize({ quantizerTestNote(60, -40, 470) }, quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_FALSE(result.events.empty());
    expectQuantizedEvent(result.events[0], 0, 480, { 60 });
}

TEST_F(MidiRecording_QuantizerTests, TwoHandChordSpreadOver55MsIsOneChord)
{
    // MuseScore's MIDI import quantizer turns this into 64th and 32nd fragments
    const std::vector<TimedNote> notes { quantizerTestNote(48, 0, 940), quantizerTestNote(64, 24, 945),
                                         quantizerTestNote(67, 53, 950) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_EQ(result.events.size(), 2u);
    expectQuantizedEvent(result.events[0], 0, 960, { 48, 64, 67 });
    expectQuantizedEvent(result.events[1], 960, 960, {});
}

TEST_F(MidiRecording_QuantizerTests, ShortNotesNeverGoBelowTheGrid)
{
    // Staccato 16ths sounding about 70 ms each: nothing finer than the 16th grid may appear
    const std::vector<TimedNote> notes { quantizerTestNote(60, 5, 72), quantizerTestNote(62, 118, 185),
                                         quantizerTestNote(64, 245, 312), quantizerTestNote(65, 352, 419) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_EQ(result.events.size(), 5u);
    expectQuantizedEvent(result.events[0], 0, 120, { 60 });
    expectQuantizedEvent(result.events[1], 120, 120, { 62 });
    expectQuantizedEvent(result.events[2], 240, 120, { 64 });
    expectQuantizedEvent(result.events[3], 360, 120, { 65 });
    expectQuantizedEvent(result.events[4], 480, 1440, {});
    for (const NotatedEvent& event : result.events) {
        EXPECT_FALSE(event.tuplet.has_value());
    }
}

TEST_F(MidiRecording_QuantizerTests, BrushIsDropped)
{
    const QuantizeResult result = quantize({ quantizerTestNote(60, 0, 480), quantizerTestNote(61, 100, 110) },
                                           quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_EQ(result.events.size(), 2u);
    expectQuantizedEvent(result.events[0], 0, 480, { 60 });
}

TEST_F(MidiRecording_QuantizerTests, ExtendMeasuresRepeatsLastSignature)
{
    const auto measures = extendMeasures({ { 0, 1440, 3, 4 } }, 4000);

    ASSERT_EQ(measures.size(), 3u);
    EXPECT_EQ(measures[2].startTick, 2880);
    EXPECT_EQ(measures[2].sigN, 3);
}

TEST_F(MidiRecording_QuantizerTests, ExtendMeasuresWithNoneStartsIn44)
{
    const auto measures = extendMeasures({}, 100);

    ASSERT_EQ(measures.size(), 1u);
    EXPECT_EQ(measures[0].ticks, 1920);
}
