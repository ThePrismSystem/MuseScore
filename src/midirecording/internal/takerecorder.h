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

#include <cstdint>
#include <optional>

#include "recordingtypes.h"
#include "takefile.h"

namespace mu::midirecording {
//! Collects one take: the MIDI events and playback position reports from
//! Record until Stop. It touches neither the score nor playback; the
//! controller drives it and reads the take back at the end.
class TakeRecorder
{
public:
    enum class State {
        Idle,
        CountIn,     // started; playback has not moved forward yet
        Recording,   // playback is moving
        Ended        // playback jumped while recording; nothing after the jump belongs to the take
    };

    enum class SampleResult {
        Ignored,            // idle, or the take has ended
        Kept,
        RecordingStarted,   // the first report that moved forward
        PlaybackMoved       // a backward step or a seek while recording: the take ends at its host time
    };

    State state() const;

    //! Started, and not yet stopped or cancelled
    bool isActive() const;

    //! Begins a take with context's start, staff, voice, modes and settings;
    //! its events, clock and stop time start empty. Any take in progress is dropped.
    void start(const TakeFile& context);

    //! Kept while counting in or recording, ignored otherwise
    void addEvent(const RawEvent& event);

    //! A jump during the count-in is kept: starting a take seeks playback to
    //! the start, and the report of that seek arrives then
    SampleResult addSample(const ClockSample& sample);

    //! Ends the take at stopNs, or where playback jumped if that came first,
    //! and returns it. Nothing while idle.
    std::optional<TakeFile> stop(int64_t stopNs);

    void cancel();

private:
    State m_state = State::Idle;
    TakeFile m_take;
    int64_t m_jumpNs = 0;
};
}
