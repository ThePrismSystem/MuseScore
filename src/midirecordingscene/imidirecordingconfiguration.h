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

#include "async/notification.h"
#include "modularity/imoduleinterface.h"

#include "midirecording/internal/recordingtypes.h"

namespace mu::midirecording {
class IMidiRecordingConfiguration : MODULE_GLOBAL_INTERFACE
{
    INTERFACE_ID(IMidiRecordingConfiguration)

public:
    virtual ~IMidiRecordingConfiguration() = default;

    virtual QuantizeSettings quantizeSettings() const = 0;

    //! "span" or "voice"
    virtual std::string replaceMode() const = 0;

    virtual int countInBars() const = 0;
    virtual int recordSpeedPercent() const = 0;
    virtual bool playOtherStaves() const = 0;
    virtual double latencyMs() const = 0;

    //! Everything the MIDI recording menu changes, read and written together
    virtual RecordingSettings recordingSettings() const = 0;
    virtual void setRecordingSettings(const RecordingSettings& recording) = 0;
    virtual muse::async::Notification settingsChanged() const = 0;

    virtual void setLatencyMs(double ms) = 0;
    virtual muse::async::Notification latencyMsChanged() const = 0;
};
}
