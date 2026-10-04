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

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "recordingtypes.h"

namespace mu::midirecording {
//! Where a take starts: chordRestTick (the selected chord or rest, or the
//! start of the outermost tuplet holding it) moved back to the straight grid
//! line at or before it, counting from the barline. The quantizer tiles from
//! the take start, so a start off the grid would give the first event an
//! odd length.
int takeStartTick(int chordRestTick, int measureStartTick, int gridTicks);

//! The tracks playback leaves out while a take records, on every staff in
//! staffIndices (the recorded staff and the staves of the same score linked
//! to it, such as a TAB staff, which would otherwise play the take back): all
//! four voices when replacing the span, the target voice alone when replacing
//! one voice, and every track when the other staves are not to be heard, so
//! that only the click sounds
std::set<size_t> takeExcludedTracks(const std::vector<size_t>& staffIndices, int voice, const std::string& replaceMode,
                                    bool playOtherStaves, size_t staffCount);
}
