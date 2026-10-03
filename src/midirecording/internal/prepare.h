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
//! An onset joins a cluster only within this and half a grid step of the cluster's latest onset
constexpr double CHORD_STEP_MS = 40.0;
//! An onset joins a cluster only within this and one grid step of the cluster's first onset
constexpr double CHORD_SPAN_MS = 60.0;

//! Drops notes held for less than brushMs: a finger grazing a neighbouring key
std::vector<TimedNote> dropBrushes(const std::vector<TimedNote>& notes, double brushMs);

//! A note starting before takeStartTick by at most half a grid step moves to
//! takeStartTick; anything earlier was played during the count-in and is dropped
std::vector<TimedNote> applyAnticipation(const std::vector<TimedNote>& notes, int takeStartTick, int gridTicks);

//! An onset joins the current cluster when it lies within half a grid step and
//! CHORD_STEP_MS of the cluster's latest onset, and within one grid step and
//! CHORD_SPAN_MS of its first, so a roll chains note by note up to a cap.
//! Every note in a cluster takes its median onset (the lower median for an
//! even count). Ordered by onset.
std::vector<TimedNote> groupChords(const std::vector<TimedNote>& notes, int gridTicks);
}
