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

static TimedNote quantizerTestNote(int pitch, int onTick, int offTick, double msPerTick = QUANTIZER_TEST_MS_PER_TICK)
{
    TimedNote note;
    note.pitch = pitch;
    note.onTick = onTick;
    note.offTick = offTick;
    note.onMs = onTick * msPerTick;
    note.heldMs = (offTick - onTick) * msPerTick;
    return note;
}

static TimedNote quantizerTestNoteMs(int pitch, int onTick, int offTick, double onMs, double offMs)
{
    TimedNote note = quantizerTestNote(pitch, onTick, offTick);
    note.onMs = onMs;
    note.heldMs = offMs - onMs;
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

static void expectNothingTied(const QuantizeResult& result)
{
    for (const NotatedEvent& event : result.events) {
        EXPECT_TRUE(event.tiedFromPrevious.empty()) << "tie at " << event.startTick;
    }
}

//! C1a: legato eighths, each released 70 ms (67 ticks) after the next onset
static std::vector<TimedNote> quantizerTestLegatoEighths()
{
    std::vector<TimedNote> notes;
    for (int i = 0; i < 4; ++i) {
        notes.push_back(quantizerTestNote(60 + 2 * i, 240 * i, 240 * i + 307));
    }
    return notes;
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

TEST_F(MidiRecording_QuantizerTests, TakeStartInsideATripletBeatGivesNoTupletBeforeIt)
{
    const std::vector<TimedNote> notes { quantizerTestNote(60, 160, 300), quantizerTestNote(62, 320, 470),
                                         quantizerTestNote(64, 480, 630), quantizerTestNote(65, 640, 790),
                                         quantizerTestNote(67, 800, 950) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 160, QuantizeSettings());

    ASSERT_FALSE(result.events.empty());
    for (const NotatedEvent& event : result.events) {
        EXPECT_GE(event.startTick, 160);
        if (event.tuplet) {
            EXPECT_GE(event.tuplet->groupStartTick, 160) << "tuplet group before the take at " << event.startTick;
        }
    }

    // The second beat still goes triplet: its error is 75 (three releases 10 ticks off, weighted 0.25)
    // against 4075 on the straight grid
    const TupletInfo group { 480, 480, 3, 2, 160 };
    int tripletEvents = 0;
    for (const NotatedEvent& event : result.events) {
        if (event.startTick == 480 || event.startTick == 640 || event.startTick == 800) {
            EXPECT_EQ(event.ticks, 160);
            EXPECT_EQ(event.tuplet, group);
            ++tripletEvents;
        }
    }
    EXPECT_EQ(tripletEvents, 3);
    expectQuantizedEvent(result.events.back(), 960, 960, {});
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
    expectQuantizedEvent(cut.events[1], 240, 240, { 62 });
    expectQuantizedEvent(cut.events[2], 480, 480, { 64 });
    expectQuantizedEvent(cut.events[3], 960, 960, {});
    expectNothingTied(cut);
}

TEST_F(MidiRecording_QuantizerTests, LegatoEighthsWithShortOverlapAreCleanEighths)
{
    const QuantizeResult result = quantize(quantizerTestLegatoEighths(), quantizerTestMeasures(2), 0, QuantizeSettings());

    // Each release snaps to the 16th past the next onset but runs only 67 ticks beyond it, so it pulls back
    ASSERT_EQ(result.events.size(), 5u);
    expectQuantizedEvent(result.events[0], 0, 240, { 60 });
    expectQuantizedEvent(result.events[1], 240, 240, { 62 });
    expectQuantizedEvent(result.events[2], 480, 240, { 64 });
    expectQuantizedEvent(result.events[3], 720, 360, { 66 });
    expectQuantizedEvent(result.events[4], 1080, 840, {});
    expectNothingTied(result);
}

TEST_F(MidiRecording_QuantizerTests, LegatoChordsWithShortOverlapAreCleanChords)
{
    // 100 bpm: a quarter lasts 600 ms. Each chord is released 80 ms (64 ticks) after the next one starts.
    const double msPerTick = 600.0 / 480.0;
    const std::vector<std::vector<int> > chords { { 48, 64, 67 }, { 50, 65, 69 }, { 48, 64, 67 } };
    std::vector<TimedNote> notes;
    for (int i = 0; i < 3; ++i) {
        for (int pitch : chords[i]) {
            notes.push_back(quantizerTestNote(pitch, 480 * i, 480 * i + 544, msPerTick));
        }
    }

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());

    // The last release, 1504, snaps to 1560 with no onset to pull back to; the 360-tick rest is not short
    ASSERT_EQ(result.events.size(), 4u);
    expectQuantizedEvent(result.events[0], 0, 480, { 48, 64, 67 });
    expectQuantizedEvent(result.events[1], 480, 480, { 50, 65, 69 });
    expectQuantizedEvent(result.events[2], 960, 600, { 48, 64, 67 });
    expectQuantizedEvent(result.events[3], 1560, 360, {});
    expectNothingTied(result);
}

TEST_F(MidiRecording_QuantizerTests, LongOverlapStillTies)
{
    // The release runs exactly one 16th (120 ticks) past the next onset, which is not short
    const std::vector<TimedNote> notes { quantizerTestNote(60, 0, 600), quantizerTestNote(62, 480, 960) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_EQ(result.events.size(), 4u);
    expectQuantizedEvent(result.events[0], 0, 480, { 60 });
    expectQuantizedEvent(result.events[1], 480, 120, { 60, 62 });
    EXPECT_EQ(result.events[1].tiedFromPrevious, std::vector<int>({ 60 }));
    expectQuantizedEvent(result.events[2], 600, 360, { 62 });
    EXPECT_EQ(result.events[2].tiedFromPrevious, std::vector<int>({ 62 }));
    expectQuantizedEvent(result.events[3], 960, 960, {});
}

TEST_F(MidiRecording_QuantizerTests, TidyOffKeepsShortOverlapTied)
{
    QuantizeSettings settings;
    settings.tidyGaps = false;

    const QuantizeResult result = quantize(quantizerTestLegatoEighths(), quantizerTestMeasures(2), 0, settings);

    // Every release snaps to the 16th after the next onset: 360, 600, 840, 1080
    ASSERT_EQ(result.events.size(), 8u);
    expectQuantizedEvent(result.events[0], 0, 240, { 60 });
    expectQuantizedEvent(result.events[1], 240, 120, { 60, 62 });
    expectQuantizedEvent(result.events[2], 360, 120, { 62 });
    expectQuantizedEvent(result.events[3], 480, 120, { 62, 64 });
    expectQuantizedEvent(result.events[4], 600, 120, { 64 });
    expectQuantizedEvent(result.events[5], 720, 120, { 64, 66 });
    expectQuantizedEvent(result.events[6], 840, 240, { 66 });
    expectQuantizedEvent(result.events[7], 1080, 840, {});
    EXPECT_EQ(result.events[1].tiedFromPrevious, std::vector<int>({ 60 }));
    EXPECT_EQ(result.events[2].tiedFromPrevious, std::vector<int>({ 62 }));
    EXPECT_EQ(result.events[6].tiedFromPrevious, std::vector<int>({ 66 }));
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

TEST_F(MidiRecording_QuantizerTests, RollStartingLateStaysOneChord)
{
    // Each onset is within 30 ms of the one before and 55 ms of the first; the median, 38, snaps to 0
    const std::vector<TimedNote> notes { quantizerTestNoteMs(48, 14, 912, 15.0, 950.0), quantizerTestNoteMs(64, 38, 917, 40.0, 955.0),
                                         quantizerTestNoteMs(67, 67, 922, 70.0, 960.0) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_EQ(result.events.size(), 2u);
    expectQuantizedEvent(result.events[0], 0, 960, { 48, 64, 67 });
    expectQuantizedEvent(result.events[1], 960, 960, {});
    expectNothingTied(result);
}

TEST_F(MidiRecording_QuantizerTests, RollAtFastTempoStartsAsOneChord)
{
    // 160 bpm. Steps of 35 ticks (27 and 28 ms) and a span of 70 ticks (55 ms) form one cluster; its median, 35, snaps to 0
    const std::vector<TimedNote> notes { quantizerTestNoteMs(48, 0, 896, 0.0, 700.0), quantizerTestNoteMs(64, 35, 902, 27.0, 705.0),
                                         quantizerTestNoteMs(67, 70, 909, 55.0, 710.0) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_FALSE(result.events.empty());
    EXPECT_EQ(result.events[0].startTick, 0);
    EXPECT_EQ(result.events[0].pitches, std::vector<int>({ 48, 64, 67 }));
    for (size_t i = 1; i < result.events.size(); ++i) {
        EXPECT_EQ(result.events[i].pitches, result.events[i].tiedFromPrevious) << "a note starts at " << result.events[i].startTick;
    }
}

TEST_F(MidiRecording_QuantizerTests, FastNotesDoNotChainIntoAChord)
{
    // 45 ms (43 ticks) apart: within half a 16th, but past the 40 ms step. Grouped, both would start at 0.
    const std::vector<TimedNote> notes { quantizerTestNoteMs(60, 30, 73, 31.25, 76.25), quantizerTestNoteMs(62, 73, 480, 76.25, 500.0) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_EQ(result.events.size(), 3u);
    expectQuantizedEvent(result.events[0], 0, 120, { 60 });
    expectQuantizedEvent(result.events[1], 120, 360, { 62 });
    expectQuantizedEvent(result.events[2], 480, 1440, {});
    expectNothingTied(result);
}

TEST_F(MidiRecording_QuantizerTests, ChainStopsAtTheSpanCap)
{
    // Onsets 25 ms (24 ticks) apart. The fourth is 75 ms from the first, past 60 ms, so it starts a new
    // cluster: the first three take their median, 24, and snap to 0; the fourth snaps from 72 to 120.
    const std::vector<TimedNote> notes { quantizerTestNoteMs(48, 0, 940, 0.0, 979.0), quantizerTestNoteMs(52, 24, 940, 25.0, 979.0),
                                         quantizerTestNoteMs(55, 48, 940, 50.0, 979.0), quantizerTestNoteMs(60, 72, 940, 75.0, 979.0) };

    const QuantizeResult result = quantize(notes, quantizerTestMeasures(2), 0, QuantizeSettings());

    ASSERT_EQ(result.events.size(), 3u);
    expectQuantizedEvent(result.events[0], 0, 120, { 48, 52, 55 });
    expectQuantizedEvent(result.events[1], 120, 840, { 48, 52, 55, 60 });
    EXPECT_EQ(result.events[1].tiedFromPrevious, std::vector<int>({ 48, 52, 55 }));
    expectQuantizedEvent(result.events[2], 960, 960, {});
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

TEST_F(MidiRecording_QuantizerTests, MeasureEndForReleaseOutsideEveryMeasureIsTheReleaseItself)
{
    const std::vector<MeasureSpan> measures = quantizerTestMeasures(2);

    EXPECT_EQ(measureEndForRelease(measures, 0), 0);
    EXPECT_EQ(measureEndForRelease(measures, 1920), 1920);
    EXPECT_EQ(measureEndForRelease(measures, 1921), 3840);
    EXPECT_EQ(measureEndForRelease(measures, 5000), 5000);
    EXPECT_EQ(measureEndForRelease({}, 700), 700);
}

TEST_F(MidiRecording_QuantizerTests, ExtendedMeasuresAreWholeMeasures)
{
    // A 4/4 score ending in a 3/4 measure: Score::appendMeasures adds whole 4/4 measures after it
    const auto measures = extendMeasures({ { 0, 1920, 4, 4 }, { 1920, 1440, 4, 4 } }, 5000);

    ASSERT_EQ(measures.size(), 3u);
    EXPECT_EQ(measures[2].startTick, 3360);
    EXPECT_EQ(measures[2].ticks, 1920);
    EXPECT_EQ(measures[2].sigN, 4);
    EXPECT_EQ(measures[2].sigD, 4);
}

TEST_F(MidiRecording_QuantizerTests, TakePastAShortFinalMeasureEndsOnAWholeMeasure)
{
    const std::vector<MeasureSpan> measures = { { 0, 1920, 4, 4 }, { 1920, 1440, 4, 4 } };
    const QuantizeResult result = quantize({ quantizerTestNote(60, 3360, 3840) }, measures, 0, QuantizeSettings());

    EXPECT_EQ(result.takeEndTick, 5280);
}
