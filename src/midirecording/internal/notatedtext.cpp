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
#include "notatedtext.h"

using namespace mu::midirecording;

static std::string notatedTextJoin(const std::vector<int>& values)
{
    std::string text;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            text += ",";
        }
        text += std::to_string(values[i]);
    }
    return text;
}

std::string mu::midirecording::notatedEventsText(const QuantizeResult& result)
{
    std::string text = "take " + std::to_string(result.takeStartTick) + " " + std::to_string(result.takeEndTick) + "\n";
    for (const NotatedEvent& event : result.events) {
        text += std::to_string(event.startTick) + " " + std::to_string(event.ticks) + " ";
        text += event.isRest() ? std::string("rest") : notatedTextJoin(event.pitches);
        if (!event.tiedFromPrevious.empty()) {
            text += " tied " + notatedTextJoin(event.tiedFromPrevious);
        }
        if (event.tuplet) {
            const TupletInfo& tuplet = *event.tuplet;
            text += " tuplet " + std::to_string(tuplet.groupStartTick) + " " + std::to_string(tuplet.groupTicks) + " "
                    + std::to_string(tuplet.actual) + ":" + std::to_string(tuplet.normal) + " " + std::to_string(tuplet.unitTicks);
        }
        text += "\n";
    }
    return text;
}

int mu::midirecording::struckNoteCount(const QuantizeResult& result)
{
    int count = 0;
    for (const NotatedEvent& event : result.events) {
        count += static_cast<int>(event.pitches.size() - event.tiedFromPrevious.size());
    }
    return count;
}
