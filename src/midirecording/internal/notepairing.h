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

#include <cstdint>
#include <functional>
#include <vector>

#include "recordingtypes.h"

namespace mu::midirecording {
using NsToTick = std::function<int (int64_t ns)>;

//! Pairs note-ons with note-offs. A note-on with velocity 0 is a note-off, a
//! note-off with no open note is ignored, a repeated note-on closes the open
//! note of that pitch first, and notes still held at stopNs end there. Events
//! after stopNs are ignored. The result is ordered by onset, then pitch.
std::vector<TimedNote> pairNotes(const std::vector<RawEvent>& events, int64_t stopNs, const NsToTick& nsToTick);
}
