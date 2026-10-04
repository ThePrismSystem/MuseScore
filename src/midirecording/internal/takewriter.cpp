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

#include "takewriter.h"

#include <optional>
#include <string>
#include <vector>

#include "engraving/dom/chord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/input.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/selectionfilter.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tuplet.h"
#include "engraving/editing/editclef.h"

#include "takesetup.h"

using namespace muse;
using namespace mu::engraving;

namespace mu::midirecording {
static Fraction takeWriterTicks(int ticks)
{
    return Fraction::fromTicks(ticks);
}

static Ret takeWriterError(const std::string& message)
{
    return make_ret(Ret::Code::UnknownError, message);
}

//! The note for a played key. Its spelling is left unset, so the note is
//! spelled from the key signature where it lands. With "use written pitch"
//! on a transposing staff out of concert pitch, the key played is the written
//! note, as in MIDI note input (Score::noteVal, which reads the input state).
static NoteVal takeWriterNoteVal(const Score* score, staff_idx_t staffIdx, int pitch, const Fraction& tick, bool useWrittenPitch)
{
    NoteVal nval(pitch);
    if (useWrittenPitch && !score->style().styleB(Sid::concertPitch)) {
        nval.pitch += score->staff(staffIdx)->part()->instrument(tick)->transpose().chromatic;
    }
    return nval;
}

//! The chords one event became: the first, and those its first note is tied
//! on to where setNoteRest split it at a beat or a barline
static std::vector<Chord*> takeWriterChain(Chord* first)
{
    std::vector<Chord*> chain { first };
    for (Note* note = first->notes().front(); note->tieFor() && note->tieFor()->endNote(); note = note->tieFor()->endNote()) {
        chain.push_back(note->tieFor()->endNote()->chord());
    }
    return chain;
}

static void takeWriterTie(Score* score, Note* from, Note* to)
{
    Tie* tie = Factory::createTie(score->dummy());
    tie->setStartNote(from);
    tie->setEndNote(to);
    tie->setTick(from->tick());
    tie->setTick2(to->tick());
    tie->setTrack(from->track());
    from->setTieFor(tie);
    to->setTieBack(tie);
    score->undoAddElement(tie);
}

//! What clearing the take's span removes: the notes and rests and everything
//! on them, but not the dynamics, hairpins, chord symbols, text, figured bass,
//! fret diagrams and lines that mark the place in the score
static SelectionFilter takeWriterClearFilter()
{
    SelectionFilter filter;
    for (ElementsSelectionFilterTypes type : { ElementsSelectionFilterTypes::DYNAMIC, ElementsSelectionFilterTypes::HAIRPIN,
                                               ElementsSelectionFilterTypes::CHORD_SYMBOL, ElementsSelectionFilterTypes::OTHER_TEXT,
                                               ElementsSelectionFilterTypes::FIGURED_BASS, ElementsSelectionFilterTypes::OTTAVA,
                                               ElementsSelectionFilterTypes::PEDAL_LINE, ElementsSelectionFilterTypes::OTHER_LINE,
                                               ElementsSelectionFilterTypes::FRET_DIAGRAM }) {
        filter.setFiltered(type, false);
    }
    return filter;
}

//! Cuts a chord or rest that sounds across tick there. A chord cut to a length
//! that needs several note values carries on, tied, through the pieces after
//! the first. changeCRlen is given the first value alone: given a length that
//! needs several, it writes the fill for the rest of the old length from the
//! end of the first value, on top of the rests it wrote for the others, and
//! nothing then starts at tick.
static Ret takeWriterCut(Score* score, ChordRest* across, const Fraction& tick, InputState& input)
{
    const track_idx_t track = across->track();
    const std::string failed = "the cut at tick " + std::to_string(tick.ticks()) + " could not be written";
    const std::vector<TDuration> values = toDurationList(tick - across->tick(), true);
    if (values.empty()) {
        return takeWriterError(failed);
    }

    std::vector<NoteVal> notes;
    if (across->isChord()) {
        for (const Note* note : toChord(across)->notes()) {
            notes.push_back(note->noteVal());
        }
    }
    score->changeCRlen(across, values.front().fraction());

    Chord* previous = across->isChord() ? toChord(across) : nullptr;
    Fraction pieceTick = across->endTick();
    for (size_t i = 1; i < values.size(); ++i) {
        Segment* segment = score->tick2segment(pieceTick, true, SegmentType::ChordRest);
        if (!segment || !segment->element(track) || !segment->element(track)->isRest()) {
            return takeWriterError(failed);
        }
        score->setNoteRest(segment, track, previous ? notes.front() : NoteVal(), values.at(i).fraction(), DirectionV::AUTO, false, {},
                           false, &input);
        if (previous) {
            ChordRest* written = score->findCR(pieceTick, track);
            if (!written || !written->isChord() || written->tick() != pieceTick) {
                return takeWriterError(failed);
            }
            Chord* chord = toChord(written);
            for (size_t j = 1; j < notes.size(); ++j) {
                score->addNote(chord, notes.at(j), false, {}, &input);
            }
            for (Note* note : previous->notes()) {
                if (Note* to = chord->findNote(note->pitch())) {
                    takeWriterTie(score, note, to);
                }
            }
            previous = chord;
        }
        pieceTick += values.at(i).fraction();
    }
    return make_ok();
}

//! A chord or rest of the track that sounds across the take's start is cut
//! there, so the take has a chord or rest to start on. Its tie on is dropped.
//! A tuplet cannot be cut.
static Ret takeWriterCutAtStart(Score* score, track_idx_t track, const Fraction& start, InputState& input)
{
    ChordRest* across = score->findCR(start, track);
    if (!across || across->tick() >= start || across->endTick() <= start) {
        return make_ok();
    }
    if (across->tuplet()) {
        return takeWriterError("the take starts inside a tuplet");
    }
    return takeWriterCut(score, across, start, input);
}

//! Replacing the span leaves voices 2 to 4 empty in it, so a rest of theirs
//! that runs across the take's start is cut there too. Their notes are kept whole.
static Ret takeWriterCutOtherVoiceRests(Score* score, track_idx_t staffTrack, const Fraction& start, InputState& input)
{
    for (track_idx_t track = staffTrack + 1; track < staffTrack + VOICES; ++track) {
        ChordRest* across = score->findCR(start, track);
        if (across && across->isRest() && !across->tuplet() && across->tick() < start && across->endTick() > start) {
            const Ret ret = takeWriterCut(score, across, start, input);
            if (!ret) {
                return ret;
            }
        }
    }
    return make_ok();
}

//! A clef change the take's span holds
struct TakeWriterClef {
    Fraction tick;
    ClefTypeList types;
};

//! The clef changes on the staff strictly inside the span. Clearing voice 1
//! removes them (deleteRange keeps only time and key signatures and
//! barlines), and a clef change governs the music after the take as well.
static std::vector<TakeWriterClef> takeWriterClefsIn(Score* score, track_idx_t staffTrack, const Fraction& start, const Fraction& end)
{
    std::vector<TakeWriterClef> clefs;
    for (Segment* segment = score->tick2segment(start, true, SegmentType::ChordRest); segment && segment->tick() < end;
         segment = segment->next1(SegmentType::Clef)) {
        EngravingItem* item = segment->element(staffTrack);
        if (segment->isClefType() && item && item->isClef() && !item->generated()) {
            clefs.push_back({ segment->tick(), toClef(item)->clefTypeList() });
        }
    }
    return clefs;
}

//! Puts a clef change back before the staff's first chord or rest at or after
//! its tick, with its concert and transposing clefs as they were
static void takeWriterRestoreClef(Score* score, staff_idx_t staffIdx, const TakeWriterClef& clef)
{
    const track_idx_t staffTrack = staffIdx * VOICES;
    Segment* segment = score->tick2rightSegment(clef.tick);
    while (segment && !segment->element(staffTrack)) {
        segment = segment->next1(SegmentType::ChordRest);
    }
    if (!segment) {
        return;
    }

    ChordRest* chordRest = toChordRest(segment->element(staffTrack));
    const bool concertPitch = score->style().styleB(Sid::concertPitch);
    score->undoChangeClef(score->staff(staffIdx), chordRest, concertPitch ? clef.types.concertClef : clef.types.transposingClef);

    // A new clef gets one type for both; a transposing instrument's pair is put back as it was
    Segment* clefSegment = chordRest->segment()->prev1(SegmentType::Clef);
    EngravingItem* restored = clefSegment && clefSegment->tick() == chordRest->tick() ? clefSegment->element(staffTrack) : nullptr;
    if (restored && restored->isClef() && !(toClef(restored)->clefTypeList() == clef.types)) {
        score->undo(new ChangeClefType(toClef(restored), clef.types.concertClef, clef.types.transposingClef));
    }
}

//! Replacing the span writes the take in voice 1 alone: the rests the clear
//! left in voices 2 to 4 go, so those voices are empty there
static void takeWriterEmptyOtherVoices(Score* score, track_idx_t staffTrack, const Fraction& start, const Fraction& end)
{
    std::vector<Rest*> rests;
    for (Segment* segment = score->tick2segment(start, true, SegmentType::ChordRest); segment && segment->tick() < end;
         segment = segment->next1(SegmentType::ChordRest)) {
        for (track_idx_t track = staffTrack + 1; track < staffTrack + VOICES; ++track) {
            EngravingItem* item = segment->element(track);
            if (item && item->isRest() && !toRest(item)->tuplet() && toRest(item)->endTick() <= end) {
                rests.push_back(toRest(item));
            }
        }
    }
    for (Rest* rest : rests) {
        score->undoRemoveElement(rest);
    }
}

//! Makes the tuplet a group of events is written into: a rest as long as the
//! group, turned into a tuplet of rests of its ratio
static Ret takeWriterMakeTuplet(Score* score, track_idx_t track, const TupletInfo& info, InputState& input)
{
    const Fraction groupStart = takeWriterTicks(info.groupStartTick);
    const Fraction groupLength = takeWriterTicks(info.groupTicks);
    Segment* segment = score->tick2segment(groupStart, true, SegmentType::ChordRest);
    if (segment && track % VOICES) {
        score->expandVoice(segment, track);
    }
    if (!TDuration::isValid(groupLength) || !segment || !segment->element(track)) {
        return takeWriterError("no place for the tuplet at tick " + std::to_string(info.groupStartTick));
    }

    score->setNoteRest(segment, track, NoteVal(), groupLength, DirectionV::AUTO, false, {}, false, &input);
    ChordRest* rest = score->findCR(groupStart, track);
    if (!rest || !rest->isRest() || rest->tick() != groupStart || rest->ticks() != groupLength) {
        return takeWriterError("no rest to hold the tuplet at tick " + std::to_string(info.groupStartTick));
    }

    const TupletNumberType numberType = TupletNumberType(score->style().styleI(Sid::tupletNumberType));
    const TupletBracketType bracketType = TupletBracketType(score->style().styleI(Sid::tupletBracketType));
    if (!score->addTuplet(rest, Fraction(info.actual, info.normal), numberType, bracketType)) {
        return takeWriterError("the tuplet at tick " + std::to_string(info.groupStartTick) + " could not be made");
    }
    return make_ok();
}

track_idx_t TakeTarget::track() const
{
    return staffIdx * VOICES + (replaceVoice ? voice : 0);
}

static Ret takeWriterWrite(Score* score, const QuantizeResult& result, const TakeTarget& target)
{
    const track_idx_t staffTrack = target.staffIdx * VOICES;
    const track_idx_t track = target.track();
    const Fraction start = takeWriterTicks(result.takeStartTick);
    const Fraction end = takeWriterTicks(result.takeEndTick);

    score->deselectAll();

    while (score->endTick() < end) {
        const Fraction before = score->endTick();
        score->appendMeasures(1);
        if (score->endTick() == before) {
            return takeWriterError("measures could not be added for the take");
        }
    }

    InputState input;
    Ret ret = takeWriterCutAtStart(score, track, start, input);
    if (!ret) {
        return ret;
    }
    if (!target.replaceVoice) {
        ret = takeWriterCutOtherVoiceRests(score, staffTrack, start, input);
        if (!ret) {
            return ret;
        }
    }

    Segment* first = score->tick2segment(start, true, SegmentType::ChordRest);
    Segment* last = score->tick2segment(end, true, SegmentType::ChordRest);
    if (!first || (!last && end < score->endTick())) {
        return takeWriterError("the take's span does not start or end on a chord or rest");
    }
    const track_idx_t clearFirst = target.replaceVoice ? track : staffTrack;
    const track_idx_t clearEnd = target.replaceVoice ? track + 1 : staffTrack + VOICES;
    // Clearing voice 1 takes the staff's clef changes with it
    const std::vector<TakeWriterClef> clefs = clearFirst == staffTrack ? takeWriterClefsIn(score, staffTrack, start, end)
                                              : std::vector<TakeWriterClef>();
    score->deleteRange(first, last, clearFirst, clearEnd, takeWriterClearFilter(), false);
    if (!target.replaceVoice) {
        takeWriterEmptyOtherVoices(score, staffTrack, start, end);
    }

    Chord* previousLast = nullptr;
    std::optional<TupletInfo> tuplet;
    for (const NotatedEvent& event : result.events) {
        const Fraction tick = takeWriterTicks(event.startTick);
        Fraction length = takeWriterTicks(event.ticks);
        if (event.tuplet) {
            if (event.tuplet != tuplet) {
                ret = takeWriterMakeTuplet(score, track, *event.tuplet, input);
                if (!ret) {
                    return ret;
                }
                tuplet = event.tuplet;
            }
            // A member is written at its nominal value: 160 ticks of an eighth-note triplet are an eighth
            length *= Fraction(event.tuplet->actual, event.tuplet->normal);
        }

        Segment* segment = score->tick2segment(tick, true, SegmentType::ChordRest);
        // A voice other than voice 1 may not exist in this measure yet
        if (segment && track % VOICES) {
            score->expandVoice(segment, track);
        }
        if (!segment || !segment->element(track)) {
            return takeWriterError("no chord or rest at tick " + std::to_string(event.startTick));
        }
        // Inside a tuplet the value is written as asked; elsewhere it is split the way the metre reads
        const bool rhythmic = !event.tuplet;
        if (event.isRest()) {
            score->setNoteRest(segment, track, NoteVal(), length, DirectionV::AUTO, false, {}, rhythmic, &input);
            previousLast = nullptr;
            continue;
        }

        score->setNoteRest(segment, track, takeWriterNoteVal(score, target.staffIdx, event.pitches.front(), tick, target.useWrittenPitch),
                           length, DirectionV::AUTO, false, {}, rhythmic, &input);
        ChordRest* written = score->findCR(tick, track);
        if (!written || !written->isChord() || written->tick() != tick) {
            return takeWriterError("the chord at tick " + std::to_string(event.startTick) + " was not written");
        }

        const std::vector<Chord*> chain = takeWriterChain(toChord(written));
        for (size_t i = 1; i < event.pitches.size(); ++i) {
            Note* previous = nullptr;
            for (Chord* chord : chain) {
                const NoteVal nval = takeWriterNoteVal(score, target.staffIdx, event.pitches.at(i), chord->tick(), target.useWrittenPitch);
                Note* note = score->addNote(chord, nval, false, {}, &input);
                if (previous) {
                    takeWriterTie(score, previous, note);
                }
                previous = note;
            }
        }

        for (int pitch : event.tiedFromPrevious) {
            const int stored = takeWriterNoteVal(score, target.staffIdx, pitch, tick, target.useWrittenPitch).pitch;
            Note* from = previousLast ? previousLast->findNote(stored) : nullptr;
            Note* to = toChord(written)->findNote(stored);
            if (!from || !to) {
                return takeWriterError("pitch " + std::to_string(pitch) + " at tick " + std::to_string(event.startTick)
                                       + " is tied from a note that is not there");
            }
            takeWriterTie(score, from, to);
        }
        previousLast = chain.back();
    }

    score->regroupNotesAndRests(start, end, track);
    for (const TakeWriterClef& clef : clefs) {
        takeWriterRestoreClef(score, target.staffIdx, clef);
    }
    return make_ok();
}

Ret writeTake(Score* score, const QuantizeResult& result, const TakeTarget& target)
{
    // Making a tuplet selects it and sets the note input duration to its value; neither is the user's
    const TDuration inputDuration = score->inputState().duration();
    const Ret ret = takeWriterWrite(score, result, target);
    score->deselectAll();
    score->inputState().setDuration(inputDuration);
    return ret;
}

int takeStartTickInScore(const Score* score, track_idx_t track, int fromTick, int gridTicks)
{
    int tick = fromTick;
    for (;;) {
        const Measure* measure = score->tick2measure(takeWriterTicks(tick));
        const int start = measure ? takeStartTick(tick, measure->tick().ticks(), gridTicks) : tick;
        const ChordRest* across = score->findCR(takeWriterTicks(start), track);
        const Tuplet* tuplet = across && across->endTick().ticks() > start ? across->topTuplet() : nullptr;
        if (!tuplet || tuplet->tick().ticks() >= start) {
            return start;
        }
        // Each pass moves back to a tuplet that starts before the last start, so this ends
        tick = tuplet->tick().ticks();
    }
}
}
