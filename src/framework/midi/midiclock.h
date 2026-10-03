/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
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

namespace muse::midi {
//! Monotonic time in nanoseconds, on the clock the MIDI input ports stamp
//! timestampedEventReceived() with. On macOS this is mach_absolute_time,
//! which pauses while the machine sleeps; std::chrono::steady_clock there
//! keeps counting, so the two must not be mixed.
int64_t midiClockNowNs();

//! Host clock ticks to nanoseconds for a numer/denom timebase, without
//! overflowing for any realistic uptime. A zero denominator gives 0.
int64_t hostTicksToNs(uint64_t ticks, uint32_t numer, uint32_t denom);
}
