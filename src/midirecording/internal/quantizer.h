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

#include <vector>

#include "recordingtypes.h"

namespace mu::midirecording {
//! Measures reaching at least toTick: past the last measure, its signature
//! repeats. No measures at all means 4/4 from tick 0.
std::vector<MeasureSpan> extendMeasures(const std::vector<MeasureSpan>& measures, int toTick);

//! End of the measure a release at tick belongs to; a release exactly on a
//! barline belongs to the measure before it
int measureEndForRelease(const std::vector<MeasureSpan>& measures, int tick);

//! The whole quantizing pass over one take: drop brushes, apply anticipation,
//! group chords, choose grids, snap, build one voice, absorb short rests. The
//! take ends at the end of the measure holding the last release.
QuantizeResult quantize(const std::vector<TimedNote>& notes, const std::vector<MeasureSpan>& measures, int takeStartTick,
                        const QuantizeSettings& settings);
}
