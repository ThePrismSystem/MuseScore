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

#include "gridselection.h"
#include "recordingtypes.h"

namespace mu::midirecording {
//! Snaps every onset to the grid of the window it falls in. With tidy gaps on,
//! a raw release that falls short of the next snapped onset by less than one
//! grid step is moved onto that onset, and a release that runs past the last
//! snapped onset before it by less than one grid step is pulled back onto that
//! onset, as long as the note keeps at least one unit of its onset's window.
//! Every other release snaps to the grid of its own window. A note never ends
//! shorter than one unit of its onset's window. A note running into the next
//! note of the same pitch ends where that note starts, and two strikes of one
//! pitch on the same grid line merge into one. Ordered by onset, then pitch.
std::vector<TimedNote> snapNotes(const std::vector<TimedNote>& notes, const std::vector<GridWindow>& windows,
                                 const QuantizeSettings& settings);
}
