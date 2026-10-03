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
#include "notepairing.h"

#include <algorithm>
#include <map>

using namespace mu::midirecording;

static constexpr double NOTEPAIRING_NS_PER_MS = 1e6;

static TimedNote notePairingMakeNote(int pitch, int64_t onNs, int64_t offNs, const NsToTick& nsToTick)
{
    TimedNote note;
    note.pitch = pitch;
    note.onTick = nsToTick(onNs);
    note.offTick = std::max(note.onTick, nsToTick(offNs));
    note.onMs = static_cast<double>(onNs) / NOTEPAIRING_NS_PER_MS;
    note.heldMs = static_cast<double>(offNs - onNs) / NOTEPAIRING_NS_PER_MS;
    return note;
}

std::vector<TimedNote> mu::midirecording::pairNotes(const std::vector<RawEvent>& events, int64_t stopNs, const NsToTick& nsToTick)
{
    std::vector<RawEvent> sorted = events;
    std::stable_sort(sorted.begin(), sorted.end(), [](const RawEvent& a, const RawEvent& b) {
        return a.ns < b.ns;
    });

    std::vector<TimedNote> result;
    std::map<int, int64_t> openOnsets;

    for (const RawEvent& event : sorted) {
        if (event.ns > stopNs) {
            break;
        }

        const bool isOn = event.on && event.velocity > 0;
        auto open = openOnsets.find(event.pitch);

        if (isOn) {
            if (open != openOnsets.end()) {
                result.push_back(notePairingMakeNote(event.pitch, open->second, event.ns, nsToTick));
                open->second = event.ns;
            } else {
                openOnsets.emplace(event.pitch, event.ns);
            }
        } else if (open != openOnsets.end()) {
            result.push_back(notePairingMakeNote(event.pitch, open->second, event.ns, nsToTick));
            openOnsets.erase(open);
        }
    }

    for (const auto& open : openOnsets) {
        result.push_back(notePairingMakeNote(open.first, open.second, stopNs, nsToTick));
    }

    std::sort(result.begin(), result.end(), [](const TimedNote& a, const TimedNote& b) {
        return a.onTick != b.onTick ? a.onTick < b.onTick : a.pitch < b.pitch;
    });

    return result;
}
