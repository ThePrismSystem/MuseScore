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
#include "timemap.h"

#include <algorithm>
#include <cmath>

using namespace mu::midirecording;

std::vector<TimeKnot> mu::midirecording::buildTimeMap(int fromTick, int toTick, int stepTicks, const TickToSecs& tickToSecs)
{
    std::vector<TimeKnot> knots;
    for (int tick = fromTick; tick < toTick; tick += stepTicks) {
        knots.push_back({ tickToSecs(tick), tick });
    }
    knots.push_back({ tickToSecs(toTick), toTick });
    return knots;
}

bool mu::midirecording::timeMapIsValid(const std::vector<TimeKnot>& knots)
{
    if (knots.size() < 2) {
        return false;
    }

    for (size_t i = 1; i < knots.size(); ++i) {
        if (knots[i].secs <= knots[i - 1].secs || knots[i].tick <= knots[i - 1].tick) {
            return false;
        }
    }
    return true;
}

int mu::midirecording::timeMapSecsToTick(const std::vector<TimeKnot>& knots, double secs)
{
    // The segment holding secs; before the first knot or past the last, the segment at that end
    const auto after = std::upper_bound(knots.cbegin() + 1, knots.cend() - 1, secs, [](double value, const TimeKnot& knot) {
        return value < knot.secs;
    });
    const TimeKnot& from = *(after - 1);
    const TimeKnot& to = *after;

    const double fraction = (secs - from.secs) / (to.secs - from.secs);
    return static_cast<int>(std::lround(from.tick + fraction * (to.tick - from.tick)));
}
