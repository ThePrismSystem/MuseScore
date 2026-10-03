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

#include "types/retval.h"

#include "recordingtypes.h"
#include "takefile.h"

namespace mu::midirecording {
using SecsToTick = std::function<int (double secs)>;

//! Turns a captured take into notation exactly as the live recorder does after
//! Stop: fit the clock map, subtract the latency compensation from each
//! event's host time, map that to playback seconds, convert to ticks, pair
//! notes and quantize. The latency is real time, so it comes off in host time,
//! before the clock map, and stays correct at any record speed. Fails when the
//! clock map is not valid.
muse::RetVal<QuantizeResult> quantizeTake(const TakeFile& take, const std::vector<MeasureSpan>& measures, const SecsToTick& secsToTick);
}
