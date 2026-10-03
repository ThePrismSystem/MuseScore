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

#include "rawevents.h"

using namespace muse::midi;
using namespace mu::midirecording;

std::vector<RawEvent> mu::midirecording::rawEventsFromMidi(int64_t ns, const Event& event)
{
    std::vector<RawEvent> result;
    for (const Event& midi10 : event.toMIDI10()) {
        const Event::Opcode opcode = midi10.opcode();
        if (opcode != Event::Opcode::NoteOn && opcode != Event::Opcode::NoteOff) {
            continue;
        }

        const int velocity = midi10.velocity7();
        const bool on = opcode == Event::Opcode::NoteOn && velocity > 0;
        result.push_back({ ns, on, midi10.note(), on ? velocity : 0 });
    }
    return result;
}
