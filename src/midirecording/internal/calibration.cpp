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
#include "calibration.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "clockmap.h"
#include "timemap.h"

using namespace mu::midirecording;

static constexpr int CALIBRATION_BEAT_TICKS = TICKS_PER_QUARTER;
static constexpr int CALIBRATION_MIN_NOTES = 4;
static constexpr double CALIBRATION_MAX_SPREAD_MS = 30.0;
static constexpr double CALIBRATION_MS_PER_SEC = 1000.0;

//! The quarter-note beats from startTick up to endTick, counted from each barline
static std::vector<int> calibrationBeats(const std::vector<MeasureSpan>& measures, int startTick, int endTick)
{
    std::vector<int> beats;
    for (const MeasureSpan& measure : measures) {
        for (int beat = measure.startTick; beat < measure.startTick + measure.ticks; beat += CALIBRATION_BEAT_TICKS) {
            if (beat >= startTick && beat < endTick) {
                beats.push_back(beat);
            }
        }
    }
    return beats;
}

//! The value a fraction p of the way through sorted, between neighbours
static double calibrationQuantile(const std::vector<double>& sorted, double p)
{
    const double position = p * static_cast<double>(sorted.size() - 1);
    const size_t below = static_cast<size_t>(position);
    const size_t above = std::min(below + 1, sorted.size() - 1);
    return sorted[below] + (position - static_cast<double>(below)) * (sorted[above] - sorted[below]);
}

CalibrationResult mu::midirecording::measureCalibration(const TakeFile& take, int endTick)
{
    CalibrationResult result;

    ClockMap clockMap;
    for (const ClockSample& sample : take.clock) {
        clockMap.addSample(sample);
    }
    if (!clockMap.isValid() || !timeMapIsValid(take.timeMap)) {
        return result;
    }

    const std::vector<int> beats = calibrationBeats(take.measures, take.startTick, endTick);
    if (beats.empty()) {
        return result;
    }

    std::vector<double> lateness;
    for (const RawEvent& event : take.events) {
        if (!event.on) {
            continue;
        }

        const double playedSecs = clockMap.secsAt(event.ns);
        const int playedTick = timeMapSecsToTick(take.timeMap, playedSecs);
        const auto nearest = std::min_element(beats.cbegin(), beats.cend(), [playedTick](int a, int b) {
            return std::abs(a - playedTick) < std::abs(b - playedTick);
        });
        // Nearer to a count-in click, or to the downbeat after the end
        if (std::abs(*nearest - playedTick) > CALIBRATION_BEAT_TICKS / 2) {
            continue;
        }

        // Playback seconds back into real time
        const double beatSecs = timeMapTickToSecs(take.timeMap, *nearest);
        lateness.push_back((playedSecs - beatSecs) / clockMap.slope() * CALIBRATION_MS_PER_SEC);
    }

    if (lateness.empty()) {
        return result;
    }

    std::sort(lateness.begin(), lateness.end());
    result.notes = static_cast<int>(lateness.size());
    result.offsetMs = calibrationQuantile(lateness, 0.5);
    result.spreadMs = calibrationQuantile(lateness, 0.75) - calibrationQuantile(lateness, 0.25);
    return result;
}

bool mu::midirecording::calibrationIsUsable(const CalibrationResult& result)
{
    return result.notes >= CALIBRATION_MIN_NOTES && result.spreadMs <= CALIBRATION_MAX_SPREAD_MS;
}
