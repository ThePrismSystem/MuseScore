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
#include "midiclock.h"

#include <chrono>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#elif defined(__linux__)
#include <time.h>
#endif

int64_t muse::midi::hostTicksToNs(uint64_t ticks, uint32_t numer, uint32_t denom)
{
    if (denom == 0) {
        return 0;
    }

    const uint64_t whole = ticks / denom;
    const uint64_t remainder = ticks % denom;
    return static_cast<int64_t>(whole * numer + remainder * numer / denom);
}

int64_t muse::midi::midiClockNowNs()
{
#if defined(__APPLE__)
    static const mach_timebase_info_data_t timebase = [] {
        mach_timebase_info_data_t info {};
        mach_timebase_info(&info);
        return info;
    }();
    return hostTicksToNs(mach_absolute_time(), timebase.numer, timebase.denom);
#elif defined(__linux__)
    timespec now {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<int64_t>(now.tv_sec) * 1000000000 + now.tv_nsec;
#else
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
