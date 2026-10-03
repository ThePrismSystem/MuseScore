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
#include <gtest/gtest.h>

#include "midi/midiclock.h"

using namespace muse::midi;

class MidiClockTests : public ::testing::Test
{
};

TEST_F(MidiClockTests, HostTicksToNs_IdentityTimebase)
{
    EXPECT_EQ(hostTicksToNs(123456789, 1, 1), 123456789);
}

TEST_F(MidiClockTests, HostTicksToNs_AppleSiliconTimebase)
{
    // 24 MHz host clock: 24'000'000 ticks are one second
    EXPECT_EQ(hostTicksToNs(24000000, 125, 3), 1000000000);
}

TEST_F(MidiClockTests, HostTicksToNs_DoesNotOverflowForLongUptime)
{
    // About ten years of uptime at 24 MHz
    const uint64_t ticks = 24000000ULL * 60 * 60 * 24 * 3650;
    EXPECT_EQ(hostTicksToNs(ticks, 125, 3), int64_t(1000000000) * 60 * 60 * 24 * 3650);
}

TEST_F(MidiClockTests, HostTicksToNs_ZeroDenominatorIsZero)
{
    EXPECT_EQ(hostTicksToNs(5, 1, 0), 0);
}

TEST_F(MidiClockTests, NowNs_NeverGoesBackwards)
{
    int64_t previous = midiClockNowNs();
    for (int i = 0; i < 1000; ++i) {
        const int64_t now = midiClockNowNs();
        EXPECT_GE(now, previous);
        previous = now;
    }
}
