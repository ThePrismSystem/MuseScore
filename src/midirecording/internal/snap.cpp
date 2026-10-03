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
#include "snap.h"

#include <algorithm>
#include <iterator>

using namespace mu::midirecording;

std::vector<TimedNote> mu::midirecording::snapNotes(const std::vector<TimedNote>& notes, const std::vector<GridWindow>& windows,
                                                    const QuantizeSettings& settings)
{
    std::vector<int> snappedOnsets;
    snappedOnsets.reserve(notes.size());
    for (const TimedNote& note : notes) {
        snappedOnsets.push_back(nearestGridLine(note.onTick, windowAt(windows, note.onTick)));
    }

    std::vector<int> onsetLines = snappedOnsets;
    std::sort(onsetLines.begin(), onsetLines.end());
    onsetLines.erase(std::unique(onsetLines.begin(), onsetLines.end()), onsetLines.end());

    std::vector<TimedNote> snapped;
    snapped.reserve(notes.size());
    for (size_t i = 0; i < notes.size(); ++i) {
        TimedNote note = notes[i];
        const int on = snappedOnsets[i];

        int off = nearestGridLine(note.offTick, windowAt(windows, note.offTick));
        if (settings.tidyGaps) {
            const auto next = std::upper_bound(onsetLines.begin(), onsetLines.end(), on);
            if (next != onsetLines.end() && note.offTick < *next && *next - note.offTick < settings.gridTicks) {
                off = *next;
            }

            const auto overlapped = std::lower_bound(onsetLines.begin(), onsetLines.end(), off);
            if (overlapped != onsetLines.begin()) {
                const int line = *std::prev(overlapped);
                if (line > on && note.offTick - line < settings.gridTicks && line - on >= windowAt(windows, on).unitTicks) {
                    off = line;
                }
            }
        }
        if (off <= on) {
            off = on + windowAt(windows, on).unitTicks;
        }

        note.onTick = on;
        note.offTick = off;
        snapped.push_back(note);
    }

    std::stable_sort(snapped.begin(), snapped.end(), [](const TimedNote& a, const TimedNote& b) {
        return a.pitch != b.pitch ? a.pitch < b.pitch : a.onTick < b.onTick;
    });

    std::vector<TimedNote> result;
    result.reserve(snapped.size());
    for (const TimedNote& note : snapped) {
        if (!result.empty() && result.back().pitch == note.pitch) {
            TimedNote& previous = result.back();
            if (previous.onTick == note.onTick) {
                previous.offTick = std::max(previous.offTick, note.offTick);
                continue;
            }
            if (previous.offTick > note.onTick) {
                previous.offTick = note.onTick;
            }
        }
        result.push_back(note);
    }

    std::sort(result.begin(), result.end(), [](const TimedNote& a, const TimedNote& b) {
        return a.onTick != b.onTick ? a.onTick < b.onTick : a.pitch < b.pitch;
    });

    return result;
}
