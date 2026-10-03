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
#include "takepipeline.h"

#include <string>

#include "clockmap.h"
#include "notepairing.h"
#include "quantizer.h"

using namespace muse;
using namespace mu::midirecording;

static constexpr double TAKEPIPELINE_MS_PER_SEC = 1000.0;

RetVal<QuantizeResult> mu::midirecording::quantizeTake(const TakeFile& take, const std::vector<MeasureSpan>& measures,
                                                       const SecsToTick& secsToTick)
{
    RetVal<QuantizeResult> result;

    ClockMap clockMap;
    for (const ClockSample& sample : take.clock) {
        clockMap.addSample(sample);
    }

    if (!clockMap.isValid()) {
        result.ret = make_ret(Ret::Code::UnknownError, std::string("take has fewer than two clock samples after playback started"));
        return result;
    }

    const double latencySecs = take.latencyMs / TAKEPIPELINE_MS_PER_SEC;
    const NsToTick nsToTick = [&](int64_t ns) {
        return secsToTick(clockMap.secsAt(ns) - latencySecs);
    };

    const std::vector<TimedNote> notes = pairNotes(take.events, take.stopNs, nsToTick);

    result.val = quantize(notes, measures, take.startTick, take.settings);
    result.ret = make_ret(Ret::Code::Ok);
    return result;
}
