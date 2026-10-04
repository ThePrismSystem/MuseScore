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

#include <functional>
#include <vector>

#include "recordingtypes.h"

namespace mu::midirecording {
using TickToSecs = std::function<double (int tick)>;

//! Samples the score's tempo map from fromTick to toTick every stepTicks
//! (above 0), with the last knot on toTick, so that a take converts to ticks
//! with no score
std::vector<TimeKnot> buildTimeMap(int fromTick, int toTick, int stepTicks, const TickToSecs& tickToSecs);

//! At least two knots, with seconds and ticks both rising
bool timeMapIsValid(const std::vector<TimeKnot>& knots);

//! The tick at secs: linear between knots, extending the first or last
//! segment beyond the ends, rounded to the nearest tick. The map must be valid.
int timeMapSecsToTick(const std::vector<TimeKnot>& knots, double secs);
}
