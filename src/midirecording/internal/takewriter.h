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

#pragma once

#include "engraving/types/types.h"
#include "types/ret.h"

#include "recordingtypes.h"

namespace mu::engraving {
class Score;
}

namespace mu::midirecording {
//! Where a take is written: one staff, and with replaceVoice one voice of it
struct TakeTarget {
    mu::engraving::staff_idx_t staffIdx = 0;
    mu::engraving::voice_idx_t voice = 0;
    bool replaceVoice = false;      // replace one voice and keep the others, instead of the whole span
    bool useWrittenPitch = false;   // on a transposing staff a played key is the written note (the MIDI input preference)

    //! The track the take's line goes on: the voice when replacing one, voice 1 otherwise
    mu::engraving::track_idx_t track() const;
};

//! Writes a quantized take into the score. The caller opens the command
//! (Score::startCmd, or the notation undo stack's prepareChanges) and closes
//! it, or rolls it back when this fails.
//!
//! Measures are appended while the take runs past the end of the score. A
//! chord or rest of the target track that sounds across the take's start is
//! cut there. The take's span is then cleared on the target staff, or on the
//! target voice alone, keeping dynamics, hairpins, chord symbols, text and
//! lines. The events are written left to right on the target track, and ties
//! and rests are regrouped the way the metre reads.
//!
//! Fails, part-way through, when an event has no chord or rest to start on
//! or a tie has no note to start from.
muse::Ret writeTake(mu::engraving::Score* score, const QuantizeResult& result, const TakeTarget& target);
}
