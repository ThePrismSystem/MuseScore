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

#include <optional>
#include <string>
#include <vector>

#include "engraving/dom/chord.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tuplet.h"
#include "engraving/types/typesconv.h"

#include "engraving/tests/utils/scorecomp.h"
#include "engraving/tests/utils/scorerw.h"

#include "midirecording/internal/takewriter.h"

using namespace muse;
using namespace mu::engraving;
using namespace mu::midirecording;

static const String TAKE_WRITER_TEST_DATA_DIR(u"takewriter/");

class MidiRecording_TakeWriterTests : public ::testing::Test
{
};

//! The voices of one staff as text, a line for each measure and voice that
//! holds anything: "m<measure> v<voice>:" then, left to right, "R:<value>" for
//! a rest and "C:<value>[<notes>]" for a chord, the value spelled as in .mscx
//! with a "." per dot. A note is "<pitch>/<tpc>", with "~" in front when it is
//! tied from the note before and "~" after it when it is tied on. A tuplet's
//! members sit between "{<actual>:<normal>" and "}".
static std::string takeWriterTestText(const Score* score, staff_idx_t staffIdx)
{
    std::string text;
    int number = 1;
    for (const Measure* measure = score->firstMeasure(); measure; measure = measure->nextMeasure(), ++number) {
        for (voice_idx_t voice = 0; voice < VOICES; ++voice) {
            const track_idx_t track = staffIdx * VOICES + voice;
            std::string line;
            const Tuplet* tuplet = nullptr;
            for (const Segment* segment = measure->first(SegmentType::ChordRest); segment;
                 segment = segment->next(SegmentType::ChordRest)) {
                const ChordRest* chordRest = segment->cr(track);
                if (!chordRest) {
                    continue;
                }
                if (chordRest->tuplet() != tuplet) {
                    if (tuplet) {
                        line += " }";
                    }
                    tuplet = chordRest->tuplet();
                    if (tuplet) {
                        line += " {" + std::to_string(tuplet->ratio().numerator()) + ":"
                                + std::to_string(tuplet->ratio().denominator());
                    }
                }
                const std::string value = std::string(TConv::toXml(chordRest->durationType().type()).ascii())
                                          + std::string(chordRest->dots(), '.');
                if (chordRest->isRest()) {
                    line += " R:" + value;
                    continue;
                }
                line += " C:" + value + "[";
                bool firstNote = true;
                for (const Note* note : toChord(chordRest)->notes()) {
                    line += std::string(firstNote ? "" : " ") + (note->tieBack() ? "~" : "") + std::to_string(note->pitch()) + "/"
                            + std::to_string(note->tpc()) + (note->tieFor() ? "~" : "");
                    firstNote = false;
                }
                line += "]";
            }
            if (tuplet) {
                line += " }";
            }
            if (!line.empty()) {
                text += "m" + std::to_string(number) + " v" + std::to_string(voice + 1) + ":" + line + "\n";
            }
        }
    }
    return text;
}

static NotatedEvent takeWriterTestEvent(int startTick, int ticks, const std::vector<int>& pitches,
                                        const std::vector<int>& tiedFromPrevious = {},
                                        const std::optional<TupletInfo>& tuplet = std::nullopt)
{
    NotatedEvent event;
    event.startTick = startTick;
    event.ticks = ticks;
    event.pitches = pitches;
    event.tiedFromPrevious = tiedFromPrevious;
    event.tuplet = tuplet;
    return event;
}

static QuantizeResult takeWriterTestTake(int takeStartTick, int takeEndTick, const std::vector<NotatedEvent>& events)
{
    QuantizeResult take;
    take.takeStartTick = takeStartTick;
    take.takeEndTick = takeEndTick;
    take.events = events;
    return take;
}

//! Loads a fixture and writes the take into it as one command, rolled back when the write fails
static MasterScore* takeWriterTestWrite(const String& fixture, const QuantizeResult& take, const TakeTarget& target, Ret& ret)
{
    MasterScore* score = ScoreRW::readScore(TAKE_WRITER_TEST_DATA_DIR + fixture);
    if (!score) {
        return nullptr;
    }
    score->startCmd(TranslatableString::untranslatable("MIDI recording tests"));
    ret = writeTake(score, take, target);
    score->endCmd(!ret);
    return score;
}

//! Starts on the off-beat inside the measure rest, ties a dotted eighth over
//! beat 2, holds a chord across the barline, ties one of its notes on, and
//! ends a measure past the end of the score
static QuantizeResult takeWriterTestStraightTake()
{
    return takeWriterTestTake(240, 5760, {
        takeWriterTestEvent(240, 360, { 66 }),
        takeWriterTestEvent(600, 120, { 67 }),
        takeWriterTestEvent(720, 240, {}),
        takeWriterTestEvent(960, 1440, { 60, 64 }),
        takeWriterTestEvent(2400, 480, { 64 }, { 64 }),
        takeWriterTestEvent(2880, 2880, {}),
    });
}

TEST_F(MidiRecording_TakeWriterTests, WritesNotesRestsChordsAndTies)
{
    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"blank-g.mscx", takeWriterTestStraightTake(), TakeTarget(), ret);
    ASSERT_TRUE(score);
    ASSERT_TRUE(ret) << ret.text();

    EXPECT_EQ(score->nmeasures(), 3);
    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: R:eighth C:eighth[66/20~] C:16th[~66/20] C:16th[67/15] R:eighth C:half[60/14~ 64/18~]\n"
              "m2 v1: C:quarter[~60/14 ~64/18~] C:quarter[~64/18] R:half\n"
              "m3 v1: R:measure\n");
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, MergesEventsTiedThroughout)
{
    const QuantizeResult take = takeWriterTestTake(0, 1920, {
        takeWriterTestEvent(0, 480, { 60 }),
        takeWriterTestEvent(480, 480, { 60 }, { 60 }),
        takeWriterTestEvent(960, 960, {}),
    });

    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"blank-g.mscx", take, TakeTarget(), ret);
    ASSERT_TRUE(score);
    ASSERT_TRUE(ret) << ret.text();

    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: C:half[60/14] R:half\n"
              "m2 v1: R:measure\n");
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, UndoPutsTheScoreBack)
{
    MasterScore* score = ScoreRW::readScore(TAKE_WRITER_TEST_DATA_DIR + u"blank-g.mscx");
    ASSERT_TRUE(score);
    ASSERT_TRUE(ScoreRW::saveScore(score, u"takewriter-undo-before.mscx"));

    score->startCmd(TranslatableString::untranslatable("MIDI recording tests"));
    const Ret ret = writeTake(score, takeWriterTestStraightTake(), TakeTarget());
    score->endCmd();
    ASSERT_TRUE(ret) << ret.text();
    ASSERT_EQ(score->nmeasures(), 3);

    score->undoRedo(true, nullptr);
    ASSERT_TRUE(ScoreRW::saveScore(score, u"takewriter-undo-after.mscx"));
    EXPECT_TRUE(ScoreComp::compareFiles(u"takewriter-undo-before.mscx", u"takewriter-undo-after.mscx"));
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, PlayedKeyIsTheWrittenNoteOnlyWithThePreference)
{
    const QuantizeResult take = takeWriterTestTake(1920, 3840, { takeWriterTestEvent(1920, 1920, { 72 }) });

    TakeTarget written;
    written.useWrittenPitch = true;
    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"handbells.mscx", take, written, ret);
    ASSERT_TRUE(score);
    ASSERT_TRUE(ret) << ret.text();
    ASSERT_EQ(score->staff(0)->part()->instrument()->transpose().chromatic, 12);
    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: C:whole[84/14]\n"
              "m2 v1: C:whole[84/14]\n");
    delete score;

    score = takeWriterTestWrite(u"handbells.mscx", take, TakeTarget(), ret);
    ASSERT_TRUE(score);
    ASSERT_TRUE(ret) << ret.text();
    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: C:whole[84/14]\n"
              "m2 v1: C:whole[72/14]\n");
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, WritesTheBassStaffOfATwoStaffPart)
{
    const QuantizeResult take = takeWriterTestTake(0, 1920, {
        takeWriterTestEvent(0, 960, { 48, 55 }),
        takeWriterTestEvent(960, 960, { 50 }),
    });

    TakeTarget bass;
    bass.staffIdx = 1;
    bass.useWrittenPitch = true;
    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"handbells.mscx", take, bass, ret);
    ASSERT_TRUE(score);
    ASSERT_TRUE(ret) << ret.text();
    ASSERT_EQ(score->staff(1)->part(), score->staff(0)->part());

    EXPECT_EQ(takeWriterTestText(score, 1),
              "m1 v1: C:half[60/14 67/15] C:half[62/16]\n"
              "m2 v1: R:measure\n");
    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: C:whole[84/14]\n"
              "m2 v1: R:measure\n");
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, OverlappingEventsAreRefused)
{
    const QuantizeResult take = takeWriterTestTake(0, 1920, {
        takeWriterTestEvent(0, 960, { 60 }),
        takeWriterTestEvent(480, 1440, { 62 }),
    });

    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"blank-g.mscx", take, TakeTarget(), ret);
    ASSERT_TRUE(score);

    EXPECT_FALSE(ret);
    EXPECT_EQ(ret.text(), "no chord or rest at tick 480");
    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: R:measure\n"
              "m2 v1: R:measure\n");
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, TieFromANoteThatIsNotThereIsRefused)
{
    const QuantizeResult take = takeWriterTestTake(0, 1920, {
        takeWriterTestEvent(0, 960, { 60 }),
        takeWriterTestEvent(960, 960, { 62 }, { 62 }),
    });

    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"blank-g.mscx", take, TakeTarget(), ret);
    ASSERT_TRUE(score);

    EXPECT_FALSE(ret);
    EXPECT_EQ(ret.text(), "pitch 62 at tick 960 is tied from a note that is not there");
    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: R:measure\n"
              "m2 v1: R:measure\n");
    delete score;
}

//! Eighth-note triplets with a rest among them, then a quarter tied into a
//! quarter-note triplet whose second member is two units long and tied on,
//! one of its two notes, out of the tuplet into the next measure
static QuantizeResult takeWriterTestTripletTake()
{
    const TupletInfo eighths { 1920, 480, 3, 2, 160 };
    const TupletInfo quarters { 2880, 960, 3, 2, 320 };
    return takeWriterTestTake(1920, 5760, {
        takeWriterTestEvent(1920, 160, { 67 }, {}, eighths),
        takeWriterTestEvent(2080, 160, {}, {}, eighths),
        takeWriterTestEvent(2240, 160, { 71 }, {}, eighths),
        takeWriterTestEvent(2400, 480, { 72 }),
        takeWriterTestEvent(2880, 320, { 72 }, { 72 }, quarters),
        takeWriterTestEvent(3200, 640, { 74, 77 }, {}, quarters),
        takeWriterTestEvent(3840, 720, { 74, 77 }, { 74 }),
        takeWriterTestEvent(4560, 1200, {}),
    });
}

TEST_F(MidiRecording_TakeWriterTests, WritesTriplets)
{
    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"blank-g.mscx", takeWriterTestTripletTake(), TakeTarget(), ret);
    ASSERT_TRUE(score);
    ASSERT_TRUE(ret) << ret.text();

    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: R:measure\n"
              "m2 v1: {3:2 C:eighth[67/15] R:eighth C:eighth[71/19] } C:quarter[72/14~]"
              " {3:2 C:quarter[~72/14] C:half[74/16~ 77/13] }\n"
              "m3 v1: C:quarter.[~74/16 77/13] R:eighth R:half\n");
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, TupletWithNoSingleNoteValueIsRefused)
{
    const QuantizeResult take = takeWriterTestTake(0, 1920, {
        takeWriterTestEvent(0, 200, { 60 }, {}, TupletInfo { 0, 600, 3, 2, 200 }),
        takeWriterTestEvent(200, 1720, {}),
    });

    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"blank-g.mscx", take, TakeTarget(), ret);
    ASSERT_TRUE(score);

    EXPECT_FALSE(ret);
    EXPECT_EQ(ret.text(), "no place for the tuplet at tick 0");
    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: R:measure\n"
              "m2 v1: R:measure\n");
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, ReapplyGivesWhatAFreshWriteGives)
{
    MasterScore* score = ScoreRW::readScore(TAKE_WRITER_TEST_DATA_DIR + u"blank-g.mscx");
    ASSERT_TRUE(score);
    score->startCmd(TranslatableString::untranslatable("MIDI recording tests"));
    ASSERT_TRUE(writeTake(score, takeWriterTestStraightTake(), TakeTarget()));
    score->endCmd();
    score->undoRedo(true, nullptr);
    score->startCmd(TranslatableString::untranslatable("MIDI recording tests"));
    ASSERT_TRUE(writeTake(score, takeWriterTestTripletTake(), TakeTarget()));
    score->endCmd();
    ASSERT_TRUE(ScoreRW::saveScore(score, u"takewriter-reapplied.mscx"));
    delete score;

    Ret ret;
    MasterScore* fresh = takeWriterTestWrite(u"blank-g.mscx", takeWriterTestTripletTake(), TakeTarget(), ret);
    ASSERT_TRUE(fresh);
    ASSERT_TRUE(ret) << ret.text();
    ASSERT_TRUE(ScoreRW::saveScore(fresh, u"takewriter-fresh.mscx"));
    delete fresh;

    EXPECT_TRUE(ScoreComp::compareFiles(u"takewriter-reapplied.mscx", u"takewriter-fresh.mscx"));
}

//! The annotations of one type on ChordRest segments in [fromTick, toTick)
static int takeWriterTestCount(const Score* score, ElementType type, int fromTick, int toTick)
{
    int count = 0;
    for (const Segment* segment = score->firstSegment(SegmentType::ChordRest); segment;
         segment = segment->next1(SegmentType::ChordRest)) {
        if (segment->tick().ticks() < fromTick || segment->tick().ticks() >= toTick) {
            continue;
        }
        for (const EngravingItem* item : segment->annotations()) {
            count += item->type() == type ? 1 : 0;
        }
    }
    return count;
}

TEST_F(MidiRecording_TakeWriterTests, ReplacingTheSpanKeepsMarkingsAndEmptiesTheOtherVoices)
{
    // The take starts inside the voice-1 half note, after the voice-2 quarter at tick 0
    const QuantizeResult take = takeWriterTestTake(480, 1920, {
        takeWriterTestEvent(480, 480, { 67 }),
        takeWriterTestEvent(960, 960, { 69 }),
    });

    MasterScore* before = ScoreRW::readScore(TAKE_WRITER_TEST_DATA_DIR + u"two-voices.mscx");
    ASSERT_TRUE(before);
    ASSERT_EQ(takeWriterTestCount(before, ElementType::DYNAMIC, 480, 1920), 1);
    ASSERT_EQ(takeWriterTestCount(before, ElementType::STAFF_TEXT, 480, 1920), 1);
    delete before;

    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"two-voices.mscx", take, TakeTarget(), ret);
    ASSERT_TRUE(score);
    ASSERT_TRUE(ret) << ret.text();

    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: C:quarter[72/14] C:quarter[67/15] C:half[69/17]\n"
              "m1 v2: C:quarter[60/14]\n"
              "m2 v1: R:measure\n");
    EXPECT_EQ(takeWriterTestCount(score, ElementType::DYNAMIC, 480, 1920), 1);
    EXPECT_EQ(takeWriterTestCount(score, ElementType::STAFF_TEXT, 480, 1920), 1);
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, ReplacingAVoiceKeepsTheOthers)
{
    const QuantizeResult take = takeWriterTestTake(0, 3840, {
        takeWriterTestEvent(0, 960, { 55 }),
        takeWriterTestEvent(960, 1440, { 57, 59 }),
        takeWriterTestEvent(2400, 1440, {}),
    });

    MasterScore* before = ScoreRW::readScore(TAKE_WRITER_TEST_DATA_DIR + u"two-voices.mscx");
    ASSERT_TRUE(before);
    ASSERT_EQ(takeWriterTestText(before, 0).find("m2 v2"), std::string::npos);
    delete before;

    TakeTarget voice2;
    voice2.voice = 1;
    voice2.replaceVoice = true;
    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"two-voices.mscx", take, voice2, ret);
    ASSERT_TRUE(score);
    ASSERT_TRUE(ret) << ret.text();

    EXPECT_EQ(takeWriterTestText(score, 0),
              "m1 v1: C:half[72/14] C:half[74/16]\n"
              "m1 v2: C:half[55/15] C:half[57/17~ 59/19~]\n"
              "m2 v1: R:measure\n"
              "m2 v2: C:quarter[~57/17 ~59/19] R:quarter R:half\n");
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, TakeStartingInsideATupletIsRefused)
{
    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"blank-g.mscx", takeWriterTestTripletTake(), TakeTarget(), ret);
    ASSERT_TRUE(score);
    ASSERT_TRUE(ret) << ret.text();
    const std::string written = takeWriterTestText(score, 0);

    // 2040 is inside the first eighth-note triplet member, 1920 to 2080
    score->startCmd(TranslatableString::untranslatable("MIDI recording tests"));
    ret = writeTake(score, takeWriterTestTake(2040, 3840, { takeWriterTestEvent(2040, 1800, {}) }), TakeTarget());
    score->endCmd(!ret);

    EXPECT_FALSE(ret);
    EXPECT_EQ(ret.text(), "the take starts inside a tuplet");
    EXPECT_EQ(takeWriterTestText(score, 0), written);
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, TakeStartMovesBackOverATuplet)
{
    Ret ret;
    MasterScore* score = takeWriterTestWrite(u"blank-g.mscx", takeWriterTestTripletTake(), TakeTarget(), ret);
    ASSERT_TRUE(score);
    ASSERT_TRUE(ret) << ret.text();

    EXPECT_EQ(takeStartTickInScore(score, 0, 2080, 120), 1920);   // second eighth-note triplet member
    EXPECT_EQ(takeStartTickInScore(score, 0, 3200, 120), 2880);   // second quarter-note triplet member
    EXPECT_EQ(takeStartTickInScore(score, 0, 2400, 120), 2400);   // the quarter between the tuplets
    delete score;
}

TEST_F(MidiRecording_TakeWriterTests, TakeStartInsideANoteStaysForTheWriterToCut)
{
    MasterScore* score = ScoreRW::readScore(TAKE_WRITER_TEST_DATA_DIR + u"two-voices.mscx");
    ASSERT_TRUE(score);

    EXPECT_EQ(takeStartTickInScore(score, 0, 600, 120), 600);   // inside the voice-1 half note, 0 to 960
    EXPECT_EQ(takeStartTickInScore(score, 0, 650, 120), 600);   // moved back to the 16th grid line
    delete score;
}
