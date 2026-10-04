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

#include "midirecordingmenu.h"

#include <cmath>

#include "midirecording/internal/settingchoices.h"

using namespace muse;
using namespace muse::actions;
using namespace mu::midirecording;

static const ActionCode MIDIRECORDINGMENU_RECORD_CODE("record-midi");
static const ActionCode MIDIRECORDINGMENU_LATENCY_RESET_CODE("midi-recording-latency-reset");
static const ActionCode MIDIRECORDINGMENU_REAPPLY_CODE("midi-recording-reapply-take");

static MidiRecordingMenuEntry midiRecordingMenuAction(const ActionCode& code)
{
    MidiRecordingMenuEntry entry;
    entry.code = code;
    return entry;
}

static MidiRecordingMenuEntry midiRecordingMenuSubmenu(const std::string& menuId, const TranslatableString& title,
                                                       const std::vector<MidiRecordingMenuEntry>& subitems)
{
    MidiRecordingMenuEntry entry;
    entry.menuId = menuId;
    entry.title = title;
    entry.subitems = subitems;
    return entry;
}

//! "Latency: +38 ms": positive when notes are moved earlier
static TranslatableString midiRecordingMenuLatencyTitle(double latencyMs)
{
    const int ms = static_cast<int>(std::lround(latencyMs));
    if (ms > 0) {
        return TranslatableString("action", "Latency: +%1 ms").arg(ms);
    }
    return TranslatableString("action", "Latency: %1 ms").arg(ms);
}

void MidiRecordingMenu::init()
{
    configuration()->latencyMsChanged().onNotify(this, [this]() {
        m_settingsMenuChanged.notify();
    });
}

ActionCode MidiRecordingMenu::recordActionCode() const
{
    return MIDIRECORDINGMENU_RECORD_CODE;
}

std::vector<MidiRecordingMenuEntry> MidiRecordingMenu::settingsMenu() const
{
    std::vector<MidiRecordingMenuEntry> entries;
    for (const SettingMenu& menu : settingMenus()) {
        if (menu.startsSection) {
            entries.push_back(MidiRecordingMenuEntry());
        }

        std::vector<MidiRecordingMenuEntry> choices;
        for (const SettingChoice& choice : menu.choices) {
            choices.push_back(midiRecordingMenuAction(choice.code));
        }

        if (menu.title.isEmpty()) {
            entries.insert(entries.end(), choices.cbegin(), choices.cend());
        } else {
            entries.push_back(midiRecordingMenuSubmenu("midi-recording-" + menu.id, menu.title, choices));
        }
    }

    entries.push_back(MidiRecordingMenuEntry());
    entries.push_back(midiRecordingMenuSubmenu("midi-recording-latency", midiRecordingMenuLatencyTitle(configuration()->latencyMs()), {
        midiRecordingMenuAction(MIDIRECORDINGMENU_LATENCY_RESET_CODE),
    }));
    entries.push_back(midiRecordingMenuAction(MIDIRECORDINGMENU_REAPPLY_CODE));
    return entries;
}

muse::async::Notification MidiRecordingMenu::settingsMenuChanged() const
{
    return m_settingsMenuChanged;
}
