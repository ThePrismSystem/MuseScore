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
#include "takesetup.h"

using namespace mu::midirecording;

int mu::midirecording::takeStartTick(int chordRestTick, int measureStartTick, int gridTicks)
{
    const int offset = chordRestTick - measureStartTick;
    return measureStartTick + offset - offset % gridTicks;
}

std::set<size_t> mu::midirecording::takeExcludedTracks(const std::vector<size_t>& staffIndices, int voice,
                                                       const std::string& replaceMode, bool playOtherStaves, size_t staffCount)
{
    std::set<size_t> tracks;
    if (!playOtherStaves) {
        for (size_t track = 0; track < staffCount * VOICES_PER_STAFF; ++track) {
            tracks.insert(track);
        }
        return tracks;
    }

    for (const size_t staffIdx : staffIndices) {
        const size_t firstTrack = staffIdx * VOICES_PER_STAFF;
        if (replaceMode == "voice") {
            tracks.insert(firstTrack + static_cast<size_t>(voice));
            continue;
        }

        for (size_t track = firstTrack; track < firstTrack + VOICES_PER_STAFF; ++track) {
            tracks.insert(track);
        }
    }
    return tracks;
}
