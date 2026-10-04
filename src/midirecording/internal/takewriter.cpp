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

#include <string>
#include <vector>

#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/input.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/selectionfilter.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tie.h"

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

//! A chord or rest of the track that sounds across the take's start is cut
//! there, so the take has a chord or rest to start on. Its tie on is dropped.
static void takeWriterCutAtStart(Score* score, track_idx_t track, const Fraction& start)
{
    ChordRest* across = score->findCR(start, track);
    if (!across || across->tick() >= start || across->endTick() <= start) {
        return;
    }
    score->changeCRlen(across, start - across->tick());
}

track_idx_t TakeTarget::track() const
{
    return staffIdx * VOICES + (replaceVoice ? voice : 0);
}

Ret writeTake(Score* score, const QuantizeResult& result, const TakeTarget& target)
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

    takeWriterCutAtStart(score, track, start);

    Segment* first = score->tick2segment(start, true, SegmentType::ChordRest);
    Segment* last = score->tick2segment(end, true, SegmentType::ChordRest);
    if (!first || (!last && end < score->endTick())) {
        return takeWriterError("the take's span does not start or end on a chord or rest");
    }
    const track_idx_t clearFirst = target.replaceVoice ? track : staffTrack;
    const track_idx_t clearEnd = target.replaceVoice ? track + 1 : staffTrack + VOICES;
    score->deleteRange(first, last, clearFirst, clearEnd, takeWriterClearFilter(), false);

    InputState input;
    Chord* previousLast = nullptr;
    for (const NotatedEvent& event : result.events) {
        const Fraction tick = takeWriterTicks(event.startTick);
        const Fraction length = takeWriterTicks(event.ticks);

        Segment* segment = score->tick2segment(tick, true, SegmentType::ChordRest);
        if (!segment || !segment->element(track)) {
            return takeWriterError("no chord or rest at tick " + std::to_string(event.startTick));
        }
        if (event.isRest()) {
            score->setNoteRest(segment, track, NoteVal(), length, DirectionV::AUTO, false, {}, true, &input);
            previousLast = nullptr;
            continue;
        }

        score->setNoteRest(segment, track, takeWriterNoteVal(score, target.staffIdx, event.pitches.front(), tick, target.useWrittenPitch),
                           length, DirectionV::AUTO, false, {}, true, &input);
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
    return make_ok();
}
}
