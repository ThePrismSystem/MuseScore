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
#include "quantizer.h"

#include <algorithm>

#include "gridselection.h"
#include "prepare.h"
#include "snap.h"
#include "voicebuilder.h"

using namespace mu::midirecording;

std::vector<MeasureSpan> mu::midirecording::extendMeasures(const std::vector<MeasureSpan>& measures, int toTick)
{
    std::vector<MeasureSpan> result = measures;
    if (result.empty()) {
        result.push_back({ 0, TICKS_PER_WHOLE, 4, 4 });
    }

    while (result.back().ticks > 0 && result.back().startTick + result.back().ticks < toTick) {
        // Whole measures of the last time signature, as Score::appendMeasures adds them, even after a short final measure
        MeasureSpan next = result.back();
        next.startTick += next.ticks;
        next.ticks = next.sigN * TICKS_PER_WHOLE / next.sigD;
        result.push_back(next);
    }

    return result;
}

int mu::midirecording::measureEndForRelease(const std::vector<MeasureSpan>& measures, int tick)
{
    for (const MeasureSpan& measure : measures) {
        const int end = measure.startTick + measure.ticks;
        if (tick > measure.startTick && tick <= end) {
            return end;
        }
    }

    return tick;
}

QuantizeResult mu::midirecording::quantize(const std::vector<TimedNote>& notes, const std::vector<MeasureSpan>& measures,
                                           int takeStartTick, const QuantizeSettings& settings)
{
    QuantizeResult result;
    result.takeStartTick = takeStartTick;
    result.takeEndTick = takeStartTick;

    std::vector<TimedNote> prepared = dropBrushes(notes, settings.brushMs);
    prepared = applyAnticipation(prepared, takeStartTick, settings.gridTicks);
    prepared = groupChords(prepared, settings.gridTicks);
    if (prepared.empty()) {
        return result;
    }

    int lastRelease = takeStartTick;
    for (const TimedNote& note : prepared) {
        lastRelease = std::max(lastRelease, note.offTick);
    }

    //! Snapping moves a release by at most half a window, so two whole notes of slack is plenty
    const std::vector<MeasureSpan> covered = extendMeasures(measures, lastRelease + 2 * TICKS_PER_WHOLE);
    const int gridEnd = covered.back().startTick + covered.back().ticks;
    const std::vector<GridWindow> windows = chooseGrids(prepared, covered, takeStartTick, gridEnd, settings);
    const std::vector<TimedNote> snapped = snapNotes(prepared, windows, settings);

    int snappedRelease = takeStartTick;
    for (const TimedNote& note : snapped) {
        snappedRelease = std::max(snappedRelease, note.offTick);
    }

    result.takeEndTick = measureEndForRelease(covered, snappedRelease);
    result.events = absorbShortRests(buildVoice(snapped, windows, takeStartTick, result.takeEndTick, settings.overlaps),
                                     settings.minRestTicks);
    return result;
}
