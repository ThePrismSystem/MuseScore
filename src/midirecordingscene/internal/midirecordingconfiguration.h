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

#include "../imidirecordingconfiguration.h"

namespace mu::midirecording {
class MidiRecordingConfiguration : public IMidiRecordingConfiguration
{
public:
    void init();

    QuantizeSettings quantizeSettings() const override;
    std::string replaceMode() const override;
    int countInBars() const override;
    int recordSpeedPercent() const override;
    bool playOtherStaves() const override;
    double latencyMs() const override;

    RecordingSettings recordingSettings() const override;
    void setRecordingSettings(const RecordingSettings& recording) override;
    muse::async::Notification settingsChanged() const override;

    void setLatencyMs(double ms) override;
    muse::async::Notification latencyMsChanged() const override;

private:
    muse::async::Notification m_settingsChanged;
    muse::async::Notification m_latencyMsChanged;
};
}
