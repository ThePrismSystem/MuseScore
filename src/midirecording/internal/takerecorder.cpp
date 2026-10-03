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

#include "takerecorder.h"

#include <algorithm>
#include <utility>

#include "clockmap.h"

using namespace mu::midirecording;

TakeRecorder::State TakeRecorder::state() const
{
    return m_state;
}

bool TakeRecorder::isActive() const
{
    return m_state != State::Idle;
}

void TakeRecorder::start(const TakeFile& context)
{
    m_take = context;
    m_take.stopNs = 0;
    m_take.events.clear();
    m_take.clock.clear();
    m_jumpNs = 0;
    m_state = State::CountIn;
}

void TakeRecorder::addEvent(const RawEvent& event)
{
    if (m_state == State::CountIn || m_state == State::Recording) {
        m_take.events.push_back(event);
    }
}

TakeRecorder::SampleResult TakeRecorder::addSample(const ClockSample& sample)
{
    if (m_state == State::Idle || m_state == State::Ended) {
        return SampleResult::Ignored;
    }

    if (m_take.clock.empty()) {
        m_take.clock.push_back(sample);
        return SampleResult::Kept;
    }

    const ClockSample previous = m_take.clock.back();

    if (m_state == State::Recording && clockSampleJumps(previous, sample)) {
        m_jumpNs = sample.hostNs;
        m_state = State::Ended;
        return SampleResult::PlaybackMoved;
    }

    m_take.clock.push_back(sample);

    if (m_state == State::CountIn && clockSampleAdvances(previous, sample)) {
        m_state = State::Recording;
        return SampleResult::RecordingStarted;
    }

    return SampleResult::Kept;
}

std::optional<TakeFile> TakeRecorder::stop(int64_t stopNs)
{
    if (m_state == State::Idle) {
        return std::nullopt;
    }

    TakeFile take = std::move(m_take);
    take.stopNs = m_state == State::Ended ? std::min(stopNs, m_jumpNs) : stopNs;

    m_take = TakeFile();
    m_state = State::Idle;
    return take;
}

void TakeRecorder::cancel()
{
    m_take = TakeFile();
    m_state = State::Idle;
}
