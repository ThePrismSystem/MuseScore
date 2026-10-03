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
#include "gridselection.h"

#include <cmath>

using namespace mu::midirecording;

bool mu::midirecording::isCompoundMeter(const MeasureSpan& measure)
{
    return measure.sigD == 8 && (measure.sigN == 6 || measure.sigN == 9 || measure.sigN == 12);
}

int mu::midirecording::nearestGridLine(int tick, const GridWindow& window)
{
    if (window.unitTicks <= 0) {
        return tick;
    }

    const double steps = static_cast<double>(tick - window.anchorTick) / window.unitTicks;
    const int nearest = static_cast<int>(std::floor(steps + 0.5));
    return window.anchorTick + nearest * window.unitTicks;
}

GridWindow mu::midirecording::windowAt(const std::vector<GridWindow>& windows, int tick)
{
    if (windows.empty()) {
        return GridWindow();
    }

    for (const GridWindow& window : windows) {
        if (tick < window.endTick) {
            return window;
        }
    }

    return windows.back();
}

static double gridSelectionFitError(const std::vector<TimedNote>& notes, const GridWindow& window)
{
    double error = 0.0;
    for (const TimedNote& note : notes) {
        if (note.onTick >= window.startTick && note.onTick < window.endTick) {
            const double distance = note.onTick - nearestGridLine(note.onTick, window);
            error += distance * distance;
        }
        if (note.offTick > window.startTick && note.offTick <= window.endTick) {
            const double distance = note.offTick - nearestGridLine(note.offTick, window);
            error += RELEASE_ERROR_WEIGHT * distance * distance;
        }
    }
    return error;
}

static int gridSelectionOffStraightOnsets(const std::vector<TimedNote>& notes, const GridWindow& straight,
                                          const GridWindow& triplet)
{
    int count = 0;
    for (const TimedNote& note : notes) {
        if (note.onTick < triplet.startTick || note.onTick >= triplet.endTick) {
            continue;
        }
        const int line = nearestGridLine(note.onTick, triplet);
        if ((line - straight.anchorTick) % straight.unitTicks != 0) {
            ++count;
        }
    }
    return count;
}

static bool gridSelectionPrefersTriplet(const std::vector<TimedNote>& notes, const GridWindow& straight,
                                        const GridWindow& triplet)
{
    if (gridSelectionOffStraightOnsets(notes, straight, triplet) < MIN_OFF_STRAIGHT_ONSETS) {
        return false;
    }

    return gridSelectionFitError(notes, triplet) < TRIPLET_ERROR_RATIO* gridSelectionFitError(notes, straight);
}

std::vector<GridWindow> mu::midirecording::chooseGrids(const std::vector<TimedNote>& notes, const std::vector<MeasureSpan>& measures,
                                                       int fromTick, int toTick, const QuantizeSettings& settings)
{
    std::vector<GridWindow> windows;
    const int windowTicks = 3 * settings.tripletUnitTicks;

    for (const MeasureSpan& measure : measures) {
        const int measureEnd = measure.startTick + measure.ticks;
        if (measureEnd <= fromTick || measure.startTick >= toTick) {
            continue;
        }

        const bool canTriplet = settings.triplets && !isCompoundMeter(measure)
                                && windowTicks > 0 && measure.ticks % windowTicks == 0;
        if (!canTriplet) {
            windows.push_back({ measure.startTick, measureEnd, settings.gridTicks, measure.startTick, false });
            continue;
        }

        for (int start = measure.startTick; start < measureEnd; start += windowTicks) {
            const GridWindow straight { start, start + windowTicks, settings.gridTicks, measure.startTick, false };
            const GridWindow triplet { start, start + windowTicks, settings.tripletUnitTicks, start, true };
            windows.push_back(gridSelectionPrefersTriplet(notes, straight, triplet) ? triplet : straight);
        }
    }

    return windows;
}
