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
#include "prepare.h"

#include <algorithm>

using namespace mu::midirecording;

std::vector<TimedNote> mu::midirecording::dropBrushes(const std::vector<TimedNote>& notes, double brushMs)
{
    std::vector<TimedNote> result;
    result.reserve(notes.size());
    for (const TimedNote& note : notes) {
        if (note.heldMs >= brushMs) {
            result.push_back(note);
        }
    }
    return result;
}

std::vector<TimedNote> mu::midirecording::applyAnticipation(const std::vector<TimedNote>& notes, int takeStartTick, int gridTicks)
{
    const int tolerance = gridTicks / 2;

    std::vector<TimedNote> result;
    result.reserve(notes.size());
    for (TimedNote note : notes) {
        if (note.onTick < takeStartTick) {
            if (takeStartTick - note.onTick > tolerance) {
                continue;
            }
            note.onTick = takeStartTick;
            note.offTick = std::max(note.offTick, takeStartTick);
        }
        result.push_back(note);
    }
    return result;
}

std::vector<TimedNote> mu::midirecording::groupChords(const std::vector<TimedNote>& notes, int gridTicks)
{
    std::vector<TimedNote> sorted = notes;
    std::stable_sort(sorted.begin(), sorted.end(), [](const TimedNote& a, const TimedNote& b) {
        if (a.onTick != b.onTick) {
            return a.onTick < b.onTick;
        }
        return a.onMs < b.onMs;
    });

    const int tickWindow = gridTicks / 4;

    size_t begin = 0;
    while (begin < sorted.size()) {
        size_t end = begin + 1;
        while (end < sorted.size()
               && sorted[end].onTick - sorted[begin].onTick <= tickWindow
               && sorted[end].onMs - sorted[begin].onMs <= CHORD_WINDOW_MS) {
            ++end;
        }

        std::vector<int> onsets;
        for (size_t i = begin; i < end; ++i) {
            onsets.push_back(sorted[i].onTick);
        }
        std::sort(onsets.begin(), onsets.end());
        const int median = onsets[(onsets.size() - 1) / 2];

        for (size_t i = begin; i < end; ++i) {
            sorted[i].onTick = median;
            sorted[i].offTick = std::max(sorted[i].offTick, median);
        }

        begin = end;
    }

    return sorted;
}
