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
#include "voicebuilder.h"

#include <algorithm>
#include <map>

using namespace mu::midirecording;

static std::vector<int> voiceBuilderSortedUnique(std::vector<int> values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

static std::vector<TimedNote> voiceBuilderCutOverlaps(std::vector<TimedNote> notes)
{
    std::vector<int> onsets;
    for (const TimedNote& note : notes) {
        onsets.push_back(note.onTick);
    }
    onsets = voiceBuilderSortedUnique(onsets);

    for (TimedNote& note : notes) {
        const auto next = std::upper_bound(onsets.begin(), onsets.end(), note.onTick);
        if (next != onsets.end()) {
            note.offTick = std::min(note.offTick, *next);
        }
    }

    std::map<int, int> chordEnd;
    for (const TimedNote& note : notes) {
        int& end = chordEnd[note.onTick];
        end = std::max(end, note.offTick);
    }
    for (TimedNote& note : notes) {
        note.offTick = chordEnd[note.onTick];
    }

    return notes;
}

static std::optional<TupletInfo> voiceBuilderTupletAt(const std::vector<GridWindow>& windows, int tick)
{
    for (const GridWindow& window : windows) {
        if (window.triplet && tick >= window.startTick && tick < window.endTick) {
            TupletInfo info;
            info.groupStartTick = window.startTick;
            info.groupTicks = window.endTick - window.startTick;
            info.unitTicks = window.unitTicks;
            return info;
        }
    }
    return std::nullopt;
}

std::vector<NotatedEvent> mu::midirecording::buildVoice(const std::vector<TimedNote>& notes, const std::vector<GridWindow>& windows,
                                                        int takeStartTick, int takeEndTick, OverlapMode mode)
{
    std::vector<TimedNote> voice = mode == OverlapMode::Cut ? voiceBuilderCutOverlaps(notes) : notes;

    std::vector<int> breakpoints { takeStartTick, takeEndTick };
    for (TimedNote& note : voice) {
        note.offTick = std::min(note.offTick, takeEndTick);
        breakpoints.push_back(note.onTick);
        breakpoints.push_back(note.offTick);
    }
    for (const GridWindow& window : windows) {
        if (window.triplet) {
            breakpoints.push_back(window.startTick);
            breakpoints.push_back(window.endTick);
        }
    }
    breakpoints = voiceBuilderSortedUnique(breakpoints);
    breakpoints.erase(std::remove_if(breakpoints.begin(), breakpoints.end(), [&](int tick) {
        return tick < takeStartTick || tick > takeEndTick;
    }), breakpoints.end());

    std::vector<NotatedEvent> events;
    for (size_t i = 0; i + 1 < breakpoints.size(); ++i) {
        NotatedEvent event;
        event.startTick = breakpoints[i];
        event.ticks = breakpoints[i + 1] - breakpoints[i];
        event.tuplet = voiceBuilderTupletAt(windows, event.startTick);

        for (const TimedNote& note : voice) {
            if (note.onTick <= event.startTick && event.startTick < note.offTick) {
                event.pitches.push_back(note.pitch);
                if (note.onTick < event.startTick) {
                    event.tiedFromPrevious.push_back(note.pitch);
                }
            }
        }
        event.pitches = voiceBuilderSortedUnique(event.pitches);
        event.tiedFromPrevious = voiceBuilderSortedUnique(event.tiedFromPrevious);

        if (event.isRest() && !events.empty() && events.back().isRest() && events.back().tuplet == event.tuplet) {
            events.back().ticks += event.ticks;
            continue;
        }

        events.push_back(event);
    }

    return events;
}

std::vector<NotatedEvent> mu::midirecording::absorbShortRests(const std::vector<NotatedEvent>& events, int minRestTicks)
{
    std::vector<NotatedEvent> result;
    result.reserve(events.size());

    for (const NotatedEvent& event : events) {
        const bool absorb = event.isRest() && !event.tuplet.has_value() && event.ticks < minRestTicks
                            && !result.empty() && !result.back().isRest() && !result.back().tuplet.has_value();
        if (absorb) {
            result.back().ticks += event.ticks;
            continue;
        }
        result.push_back(event);
    }

    return result;
}
