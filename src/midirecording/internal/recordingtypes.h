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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace mu::midirecording {
//! Mirrors mu::engraving::Constants::DIVISION
constexpr int TICKS_PER_QUARTER = 480;
constexpr int TICKS_PER_WHOLE = 4 * TICKS_PER_QUARTER;

//! Mirrors mu::engraving::VOICES
constexpr size_t VOICES_PER_STAFF = 4;

//! A note-on or note-off as captured, in nanoseconds from the take's time origin
struct RawEvent {
    int64_t ns = 0;
    bool on = false;
    int pitch = 0;
    int velocity = 0;
};

//! A playback position report and the host time it arrived, on the RawEvent clock
struct ClockSample {
    int64_t hostNs = 0;
    double playbackSecs = 0.0;
};

//! A point on a take's tempo map: playback seconds at a score tick
struct TimeKnot {
    double secs = 0.0;
    int tick = 0;
};

//! A paired note in score ticks, keeping real time for the thresholds that are physical
struct TimedNote {
    int pitch = 0;
    int onTick = 0;
    int offTick = 0;
    double onMs = 0.0;
    double heldMs = 0.0;
};

struct MeasureSpan {
    int startTick = 0;
    int ticks = 0;
    int sigN = 4;
    int sigD = 4;
};

enum class OverlapMode {
    Tied,
    Cut
};

struct QuantizeSettings {
    int gridTicks = 120;          // 16th
    bool triplets = true;
    int tripletUnitTicks = 160;   // 8th-note triplet
    bool tidyGaps = true;         // closes short gaps and short overlaps at onsets
    int minRestTicks = 240;       // 8th
    double brushMs = 40.0;
    OverlapMode overlaps = OverlapMode::Tied;
};

struct TupletInfo {
    int groupStartTick = 0;
    int groupTicks = 0;
    int actual = 3;
    int normal = 2;
    int unitTicks = 0;
};

inline bool operator==(const TupletInfo& a, const TupletInfo& b)
{
    return a.groupStartTick == b.groupStartTick && a.groupTicks == b.groupTicks
           && a.actual == b.actual && a.normal == b.normal && a.unitTicks == b.unitTicks;
}

inline bool operator!=(const TupletInfo& a, const TupletInfo& b)
{
    return !(a == b);
}

//! One chord or rest of the written voice
struct NotatedEvent {
    int startTick = 0;
    int ticks = 0;
    std::vector<int> pitches;           // ascending; empty means a rest
    std::vector<int> tiedFromPrevious;  // ascending subset of pitches
    std::optional<TupletInfo> tuplet;

    bool isRest() const { return pitches.empty(); }
};

struct QuantizeResult {
    std::vector<NotatedEvent> events;
    int takeStartTick = 0;
    int takeEndTick = 0;
};
}
