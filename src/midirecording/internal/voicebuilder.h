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

#include <vector>

#include "gridselection.h"
#include "recordingtypes.h"

namespace mu::midirecording {
//! Turns snapped notes into one voice: consecutive chords and rests covering
//! [takeStartTick, takeEndTick). Every onset and release is a breakpoint, and
//! so is every edge of a triplet window, so events never cross a triplet
//! group's edge and every event inside one carries its TupletInfo.
//! Tied: a pitch sounding across a breakpoint continues as a tied note in the
//! next event. Cut: a note ends at the next onset, and the notes of one chord
//! share its longest length. Adjacent rests in the same group merge.
std::vector<NotatedEvent> buildVoice(const std::vector<TimedNote>& notes, const std::vector<GridWindow>& windows, int takeStartTick,
                                     int takeEndTick, OverlapMode mode);

//! Folds each rest shorter than minRestTicks into the chord before it. A rest
//! with no chord before it, a rest inside a triplet group, and a rest after a
//! triplet chord stay as they are.
std::vector<NotatedEvent> absorbShortRests(const std::vector<NotatedEvent>& events, int minRestTicks);
}
