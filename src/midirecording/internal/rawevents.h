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
#include <vector>

#include "midi/midievent.h"

#include "recordingtypes.h"

namespace mu::midirecording {
//! The note-ons and note-offs in one MIDI message, stamped ns. MIDI 2.0
//! messages are converted to MIDI 1.0 first, the way note input does it; a
//! note-on with velocity 0 is a note-off. Every other message gives nothing.
std::vector<RawEvent> rawEventsFromMidi(int64_t ns, const muse::midi::Event& event);
}
