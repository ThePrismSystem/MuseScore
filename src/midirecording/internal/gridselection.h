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
//! A triplet grid must beat the straight grid's error by this factor
constexpr double TRIPLET_ERROR_RATIO = 0.5;
//! A release counts for this fraction of an onset in the fit error
constexpr double RELEASE_ERROR_WEIGHT = 0.25;
//! Onsets that must sit off the straight grid before a window can go triplet
constexpr int MIN_OFF_STRAIGHT_ONSETS = 2;

//! A stretch of the score with one grid: lines at anchorTick + k * unitTicks
struct GridWindow {
    int startTick = 0;
    int endTick = 0;
    int unitTicks = 0;
    int anchorTick = 0;
    bool triplet = false;

    bool operator==(const GridWindow& other) const
    {
        return startTick == other.startTick && endTick == other.endTick && unitTicks == other.unitTicks
               && anchorTick == other.anchorTick && triplet == other.triplet;
    }
};

//! 6/8, 9/8 and 12/8: the beat already divides in three
bool isCompoundMeter(const MeasureSpan& measure);

//! The grid line nearest to tick; exactly halfway rounds to the later line
int nearestGridLine(int tick, const GridWindow& window);

//! The window containing tick. Ticks before the first window map to the first
//! and ticks past the last map to the last; an empty list gives a default window.
GridWindow windowAt(const std::vector<GridWindow>& windows, int tick);

//! Tiles every measure overlapping [fromTick, toTick) with grid windows. With
//! triplets on, in a simple meter whose length is a multiple of three triplet
//! units, a measure is cut into windows of three triplet units from its start,
//! and each window takes the triplet grid only when at least
//! MIN_OFF_STRAIGHT_ONSETS of its onsets sit off the straight grid and the
//! triplet fit error is below TRIPLET_ERROR_RATIO of the straight one.
//! Otherwise the measure is a single straight window.
std::vector<GridWindow> chooseGrids(const std::vector<TimedNote>& notes, const std::vector<MeasureSpan>& measures, int fromTick, int toTick,
                                    const QuantizeSettings& settings);
}
