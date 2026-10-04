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

#include "takefile.h"

namespace mu::midirecording {
//! How late a calibration take's notes came after the beats they were played on
struct CalibrationResult {
    int notes = 0;           // the notes measured
    double offsetMs = 0.0;   // their median lateness, in real time; negative when early
    double spreadMs = 0.0;   // the interquartile range of their lateness
};

//! Measures a calibration take: every note-on is compared with the nearest
//! quarter-note beat from the take's start up to endTick, the beats counted
//! from each barline. A note more than half a beat from all of them (played
//! along with the count-in, or after the end) is left out. The take's own
//! latency is ignored, so the result is the whole latency. A take whose clock
//! or time map is not valid measures no notes.
CalibrationResult measureCalibration(const TakeFile& take, int endTick);

//! At least 4 notes, spread within 30 ms
bool calibrationIsUsable(const CalibrationResult& result);
}
