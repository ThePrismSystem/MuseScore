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

#include "midirecordinguiactions.h"

#include "context/shortcutcontext.h"
#include "context/uicontext.h"
#include "types/translatablestring.h"
#include "ui/view/iconcodes.h"

using namespace muse;
using namespace muse::actions;
using namespace muse::ui;
using namespace mu::midirecording;

static const ActionCode RECORD_MIDI_ACTION_CODE("record-midi");

const UiActionList MidiRecordingUiActions::s_actions = {
    UiAction(RECORD_MIDI_ACTION_CODE,
             mu::context::UiCtxProjectOpened,
             mu::context::CTX_NOTATION_FOCUSED,
             TranslatableString("action", "Record MIDI"),
             TranslatableString("action", "Record MIDI from the selected note or rest"),
             IconCode::Code::RECORD_FILL,
             Checkable::Yes
             ),
    UiAction("midi-recording-export-take",
             mu::context::UiCtxAny,
             mu::context::CTX_ANY,
             TranslatableString("action", "Export last MIDI take…")
             ),
    UiAction("midi-recording-replay-take",
             mu::context::UiCtxAny,
             mu::context::CTX_ANY,
             TranslatableString("action", "Replay MIDI take file…")
             ),
};

MidiRecordingUiActions::MidiRecordingUiActions(std::shared_ptr<MidiRecordingController> controller)
    : m_controller(controller)
{
}

void MidiRecordingUiActions::init()
{
    m_controller->isRecordingChanged().onNotify(this, [this]() {
        m_actionCheckedChanged.send({ RECORD_MIDI_ACTION_CODE });
    });
}

const UiActionList& MidiRecordingUiActions::actionsList() const
{
    return s_actions;
}

bool MidiRecordingUiActions::actionEnabled(const UiAction&) const
{
    return true;
}

async::Channel<ActionCodeList> MidiRecordingUiActions::actionEnabledChanged() const
{
    return m_actionEnabledChanged;
}

bool MidiRecordingUiActions::actionChecked(const UiAction& act) const
{
    return act.code == RECORD_MIDI_ACTION_CODE && m_controller->isRecording();
}

async::Channel<ActionCodeList> MidiRecordingUiActions::actionCheckedChanged() const
{
    return m_actionCheckedChanged;
}
