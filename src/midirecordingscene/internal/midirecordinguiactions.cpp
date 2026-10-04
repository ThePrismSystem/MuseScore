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

#include "midirecording/internal/settingchoices.h"

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
    UiAction("midi-recording-reapply-take",
             mu::context::UiCtxProjectOpened,
             mu::context::CTX_ANY,
             TranslatableString("action", "Re-apply to last MIDI take")
             ),
    UiAction("midi-recording-latency-reset",
             mu::context::UiCtxAny,
             mu::context::CTX_ANY,
             TranslatableString("action", "Reset to 0 ms"),
             TranslatableString("action", "Reset MIDI recording latency to 0 ms")
             ),
};

MidiRecordingUiActions::MidiRecordingUiActions(std::shared_ptr<MidiRecordingController> controller)
    : m_controller(controller), m_actions(s_actions)
{
    for (const SettingMenu& menu : settingMenus()) {
        for (const SettingChoice& choice : menu.choices) {
            m_actions.push_back(UiAction(choice.code, mu::context::UiCtxAny, mu::context::CTX_ANY, choice.title, choice.description,
                                         Checkable::Yes));
        }
    }
}

void MidiRecordingUiActions::init()
{
    m_controller->isRecordingChanged().onNotify(this, [this]() {
        m_actionCheckedChanged.send({ RECORD_MIDI_ACTION_CODE });
        m_actionEnabledChanged.send({ RECORD_MIDI_ACTION_CODE });
    });

    m_controller->canToggleRecordChanged().onNotify(this, [this]() {
        m_actionEnabledChanged.send({ RECORD_MIDI_ACTION_CODE });
    });

    configuration()->settingsChanged().onNotify(this, [this]() {
        ActionCodeList codes;
        for (const SettingMenu& menu : settingMenus()) {
            for (const SettingChoice& choice : menu.choices) {
                codes.push_back(choice.code);
            }
        }
        m_actionCheckedChanged.send(codes);
    });
}

const UiActionList& MidiRecordingUiActions::actionsList() const
{
    return m_actions;
}

bool MidiRecordingUiActions::actionEnabled(const UiAction& act) const
{
    if (act.code == RECORD_MIDI_ACTION_CODE) {
        return m_controller->canToggleRecord();
    }
    return true;
}

async::Channel<ActionCodeList> MidiRecordingUiActions::actionEnabledChanged() const
{
    return m_actionEnabledChanged;
}

bool MidiRecordingUiActions::actionChecked(const UiAction& act) const
{
    if (act.code == RECORD_MIDI_ACTION_CODE) {
        return m_controller->isRecording();
    }

    const SettingChoice* choice = findSettingChoice(act.code);
    return choice && choice->isChosen(configuration()->recordingSettings());
}

async::Channel<ActionCodeList> MidiRecordingUiActions::actionCheckedChanged() const
{
    return m_actionCheckedChanged;
}
