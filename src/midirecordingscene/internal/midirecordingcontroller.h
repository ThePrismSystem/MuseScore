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
#include <string>

#include "actions/actionable.h"
#include "actions/iactionsdispatcher.h"
#include "async/asyncable.h"
#include "async/notification.h"
#include "context/iglobalcontext.h"
#include "global/iglobalconfiguration.h"
#include "iinteractive.h"
#include "io/ifilesystem.h"
#include "midi/imidiinport.h"
#include "modularity/ioc.h"
#include "notation/imasternotation.h"
#include "notation/inotationconfiguration.h"
#include "playback/iplaybackconfiguration.h"
#include "playback/iplaybackcontroller.h"

#include "midirecording/internal/takefile.h"
#include "midirecording/internal/takerecorder.h"

#include "../imidirecordingconfiguration.h"

namespace mu::midirecording {
//! Runs a take from the Record MIDI action: checks that one can start, sets
//! playback up for it (count-in, click, the recorded staff silent, record
//! speed, repeats off), feeds MIDI and position reports to a TakeRecorder,
//! and puts playback back as it was once playback has stopped. The finished
//! take is quantized and reported; writing it into the score comes later.
class MidiRecordingController : public muse::actions::Actionable, public muse::async::Asyncable, public muse::Contextable
{
    muse::GlobalInject<IMidiRecordingConfiguration> configuration;
    muse::GlobalInject<playback::IPlaybackConfiguration> playbackConfiguration;
    muse::GlobalInject<notation::INotationConfiguration> notationConfiguration;
    muse::GlobalInject<muse::IGlobalConfiguration> globalConfiguration;
    muse::GlobalInject<muse::io::IFileSystem> fileSystem;
    muse::ContextInject<muse::actions::IActionsDispatcher> dispatcher = { this };
    muse::ContextInject<context::IGlobalContext> globalContext = { this };
    muse::ContextInject<playback::IPlaybackController> playbackController = { this };
    muse::ContextInject<muse::midi::IMidiInPort> midiInPort = { this };
    muse::ContextInject<muse::IInteractive> interactive = { this };

public:
    explicit MidiRecordingController(const muse::modularity::ContextPtr& iocCtx);

    void init();

    //! A take has started and has not finished or been cancelled
    bool isRecording() const;
    muse::async::Notification isRecordingChanged() const;

private:
    void toggleRecord();
    void startTake();
    void requestStop();
    void finishTake();
    void cancelTake();
    void dropTake();
    void restorePlayback();
    void forgetClosedScore();
    void forgetTakeState();
    void reportTake(const TakeFile& take);
    void exportTake();
    void replayTake();
    void refuse(const std::string& reason);

    void onPositionChanged(double secs);
    void onPlayingChanged();
    void onDeviceChanged();
    void onNotationChanged();

    TakeRecorder m_recorder;
    notation::IMasterNotationPtr m_takeMasterNotation;
    muse::midi::MidiDeviceID m_takeDeviceId;
    bool m_sawPlaying = false;
    bool m_restorePending = false;
    bool m_metronomeForced = false;
    bool m_repeatsForced = false;
    bool m_midiInputForced = false;
    bool m_playNotesWhenEditingForced = false;
    bool m_playNotesOnMidiInputForced = false;
    double m_savedTempoMultiplier = 1.0;
    std::optional<int64_t> m_stopNs;
    std::optional<TakeFile> m_lastTake;
    muse::async::Notification m_isRecordingChanged;
};
}
