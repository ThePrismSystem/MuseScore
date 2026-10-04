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

#include <string>

#include "recordingtypes.h"

namespace mu::midirecording {
//! A quantized take as text, one line for the take and one per event:
//!   take <takeStartTick> <takeEndTick>
//!   <startTick> <ticks> rest
//!   <startTick> <ticks> <pitch>,<pitch>[ tied <pitch>,<pitch>][ tuplet <groupStartTick> <groupTicks> <actual>:<normal> <unitTicks>]
//! Recorded takes are checked against this text, so the format changes only
//! together with every expected file.
std::string notatedEventsText(const QuantizeResult& result);

//! Notes struck in the take: every pitch not tied from the event before
int struckNoteCount(const QuantizeResult& result);
}
