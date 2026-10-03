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
#include <string>
#include <vector>

#include "types/bytearray.h"
#include "types/retval.h"

#include "recordingtypes.h"

namespace mu::midirecording {
//! Everything needed to reproduce a take offline. Times are nanoseconds from
//! the take's time origin; JSON stores them as decimal strings, since the bundled JSON
//! writer prints a whole number through int and would cut a time past about 2.1 s to 32 bits.
//! playbackSecs and the other fractional values round-trip to six decimal places (1 us).
struct TakeFile {
    static constexpr int CURRENT_VERSION = 1;

    int version = CURRENT_VERSION;
    int startTick = 0;
    int staffIdx = 0;
    int voice = 0;
    std::string replaceMode = "span";   // "span" or "voice"
    int countInBars = 1;
    int recordSpeedPercent = 100;
    double latencyMs = 0.0;
    int64_t stopNs = 0;
    QuantizeSettings settings;
    std::vector<RawEvent> events;
    std::vector<ClockSample> clock;
    std::vector<MeasureSpan> measures;   // from the take's first measure to the end of the score
    std::vector<TimeKnot> timeMap;       // playback seconds at score ticks, while the take ran
};

muse::ByteArray takeToJson(const TakeFile& take);

//! Fails on invalid JSON, an unknown version, a missing top-level key, an
//! event, clock, measure or time map entry with the wrong number of fields,
//! a time that is not a whole number in a string, a measure whose length or
//! time signature is not above 0, or a setting out of range: gridTicks or
//! tripletUnitTicks not above 0, minRestTicks or brushMs negative. Missing
//! settings keys take the QuantizeSettings defaults.
muse::RetVal<TakeFile> takeFromJson(const muse::ByteArray& data);
}
